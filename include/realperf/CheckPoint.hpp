#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include "DoubleMapBuffer.hpp"
#include "LiteralString.hpp"
#include "TickTimer.hpp"
#include <string>
#include <x86intrin.h>

namespace realperf {

using Tick = std::uint64_t;

enum Category : std::uint8_t
{
    CAT_Default = 0,
    CAT_MD = 1,
    CAT_Order = 2,
    CAT_Strategy = 3,
    CAT_Event = 4,
};

struct alignas(16) CheckPoint
{
    enum class Type : std::uint8_t
    {
        CP_Start,
        CP_Normal,
        CP_ScopeStart,
        CP_ScopeEnd,
        CP_Order,
        CP_Event,
        CP_Custom,
        CP_End,
    };

    Type type_;
    Category category_;
    uint16_t reserved_;
    LiteralString where_;
    Tick tick_;
};
static_assert(sizeof(CheckPoint) == 16, "CheckPoint size must be 16 bytes");
static_assert(alignof(CheckPoint) == 16, "CheckPoint alignment must be 16 bytes");
static_assert(offsetof(CheckPoint, type_) == 0, "CheckPoint type offset must be 0");
static_assert(offsetof(CheckPoint, category_) == 1, "CheckPoint category offset must be 1");
static_assert(offsetof(CheckPoint, reserved_) == 2, "CheckPoint reserved offset must be 2");
static_assert(offsetof(CheckPoint, where_) == 4, "CheckPoint where offset must be 4");
static_assert(offsetof(CheckPoint, tick_) == 8, "CheckPoint tick offset must be 8");

struct alignas(16) OrderCheckPoint : public CheckPoint
{
    OrderCheckPoint(uint16_t symbolID, double price, uint32_t quantity, Tick tick = TickTimer::now())
    {
        type_ = Type::CP_Order;
        category_ = Category::CAT_Order;
        reserved_ = sizeof(OrderCheckPoint) - sizeof(CheckPoint);
        where_ = LiteralString::fromLiteral("OrderCheckPoint");
        tick_ = tick;

        symbolID_ = symbolID;
        price_ = price;
        quantity_ = quantity;
    }
    double price_;
    uint32_t quantity_;
    uint16_t symbolID_;
    uint16_t reserved2_;
};
static_assert(sizeof(OrderCheckPoint) == 32, "OrderCheckPoint size must be 32 bytes");

template <typename Event, typename... Args>
struct alignas(16) EventCheckPoint : public CheckPoint, public Event
{
    template <typename... EventArgs>
    EventCheckPoint(Tick tick, LiteralString eventName, EventArgs&&... eventArgs): Event(std::forward<EventArgs>(eventArgs)...)
    {
        type_ = Type::CP_Event;
        category_ = Category::CAT_Event;
        reserved_ = sizeof(Event);
        where_ = eventName; // this is used to identify Event type, so that we can print it later
        tick_ = tick;
    }
};

struct Recorder
{

    Recorder()
    {
        auto& buffer = dummyBuffer();
        init(buffer.begin(), buffer.end());
    }

    inline static Recorder& instance()
    {
        thread_local Recorder record;
        return record;
    }

    // have init so that parameters can be read later time
    void init( CheckPoint * begin, CheckPoint * end, unsigned minCount = 4, unsigned mustHaveCategories = 0)
    {
        start_ = begin;
        count_ = 0;
        categories_ = 0;
        end_ = end;
        capacity_ = end - begin;
        minCount_ = minCount;
        mustHaveCategories_ = mustHaveCategories;

        // verify that we are using double mapped ring buffer
        // benefit of using double mapped buffer is that we can write whole record without worrying about wrapping around
        // we only need to adjust pointer at commit
        end[1].tick_ = 123456789ull;
        if (begin[1].tick_ != 123456789ull)
        {
            throw std::runtime_error("Recorder must be initialized with a double mapped ring buffer");
        }
        ::memset(begin, 0, capacity_ * sizeof(CheckPoint));
    }

    Tick totalTicks() const
    {
        if (count_ == 0) {
            return 0;
        }

        const CheckPoint& last = start_[count_ - 1];
        return last.tick_ - start_->tick_;
    }

    bool has(Category category) const
    {
        return (categories_ & (1u << static_cast<std::uint8_t>(category))) != 0;
    }

    void rollback()
    {
        count_ = 0;
        categories_ = 0;
    }

    void commit()
    {
        start_ += (count_ >= minCount_) ? count_ : 0;
        count_ = 0;
        categories_ = 0;

        // keep offset the same, because we are using a double mapped ring buffer
        if (start_ >= end_) [[unlikely]]
        {
            start_ -= capacity_;
        }
    }

    void commitIfHas(Category category)
    {
        if (has(category)) {
            commit();
        } else {
            rollback();
        }
    }

    // starting a new recording
    void start(LiteralString where, Tick tick = TickTimer::now())
    {
        commit(); //always commit previous recording before starting a new one
        add(tick, where, CheckPoint::Type::CP_Start);
    }

    void end(LiteralString where, Tick tick = TickTimer::now())
    {
        add(tick, where, CheckPoint::Type::CP_End);
        commit();
    }

    CheckPoint& add(Tick tick, LiteralString where, CheckPoint::Type type=CheckPoint::Type::CP_Normal, Category category= Category::CAT_Default)
    {
        CheckPoint& cp = start_[count_];

        // write CheckPoint atomically using a 128-bit store
        const std::uint64_t parts =
            static_cast<std::uint64_t>(static_cast<std::uint8_t>(type))
            | (static_cast<std::uint64_t>(static_cast<std::uint8_t>(category)) << 8u)
            | (static_cast<std::uint64_t>(where.value()) << 32u);
        const __m128i checkpoint = _mm_set_epi64x(tick, parts);
        _mm_store_si128(reinterpret_cast<__m128i*>(&cp), checkpoint);

        ++count_;
        categories_ |= (1u << static_cast<std::uint8_t>(category));
        return cp;
    }

    CheckPoint& add(LiteralString where, CheckPoint::Type type=CheckPoint::Type::CP_Normal, Category category= Category::CAT_Default)
    {
        return add(TickTimer::now(), where, type, category);
    }

    template <typename Event, typename... EventArgs>
    EventCheckPoint<Event>& addEvent(Tick tick, LiteralString eventName, EventArgs&&... eventArgs)
    {
        using StoredCheckPoint = EventCheckPoint<Event>;
        static_assert(alignof(StoredCheckPoint) <= alignof(CheckPoint), "EventCheckPoint alignment exceeds recorder storage alignment");
        static_assert(sizeof(StoredCheckPoint) % sizeof(CheckPoint) == 0, "EventCheckPoint size must be a multiple of CheckPoint size");
        static_assert(std::is_trivially_destructible_v<StoredCheckPoint>, "EventCheckPoint must be trivially destructible");

        auto& cp = *::new (static_cast<void*>(start_ + count_)) StoredCheckPoint(tick, eventName, std::forward<EventArgs>(eventArgs)...);
        count_ += static_cast<std::uint16_t>(sizeof(StoredCheckPoint) / sizeof(CheckPoint));
        categories_ |= (1u << static_cast<std::uint8_t>(cp.category_));
        return cp;
    }

    template <typename Event, typename... EventArgs>
    EventCheckPoint<Event>& addEvent(LiteralString eventName, EventArgs&&... eventArgs)
    {
        return addEvent<Event>(TickTimer::now(), eventName, std::forward<EventArgs>(eventArgs)...);
    }

private:
    struct DummyBuffer
    {
        DummyBuffer()
            : buffer_(sizeof(CheckPoint) * 256u)
        {
            if (buffer_.capacity() % sizeof(CheckPoint) != 0u) {
                throw std::runtime_error("Dummy recorder buffer capacity must align to CheckPoint size");
            }
        }

        CheckPoint* begin()
        {
            return static_cast<CheckPoint*>(buffer_.buffer());
        }

        CheckPoint* end()
        {
            return begin() + (buffer_.capacity() / sizeof(CheckPoint));
        }

        DoubleMapBuffer buffer_;
    };

    static DummyBuffer& dummyBuffer()
    {
        static thread_local DummyBuffer buffer;
        return buffer;
    }

    CheckPoint * start_ = nullptr;
    std::uint16_t count_ = 0;
    std::uint16_t categories_ = 0;
    std::uint16_t minCount_ = 4;
    std::uint16_t mustHaveCategories_ = 0;
    CheckPoint * end_ = nullptr;
    std::size_t capacity_ = 0;
};

class CheckPointScope
{
public:
    explicit CheckPointScope(LiteralString where, Category category = Category::CAT_Default)
        : CheckPointScope(Recorder::instance(), where, category)
    {
    }

    explicit CheckPointScope(Recorder& recorder, LiteralString where, Category category = Category::CAT_Default)
        : recorder_(recorder)
        , where_(where)
        , category_(category)
    {
        recorder_.add(where_, CheckPoint::Type::CP_ScopeStart, category_);
    }

    CheckPointScope(const CheckPointScope&) = delete;
    CheckPointScope& operator=(const CheckPointScope&) = delete;
    CheckPointScope(CheckPointScope&&) = delete;
    CheckPointScope& operator=(CheckPointScope&&) = delete;

    ~CheckPointScope()
    {
        recorder_.add(where_, CheckPoint::Type::CP_ScopeEnd, category_);
    }

private:
    Recorder& recorder_;
    LiteralString where_;
    Category category_;
};

#define REALPERF_DETAIL_CONCAT_IMPL(left, right) left##right
#define REALPERF_DETAIL_CONCAT(left, right) REALPERF_DETAIL_CONCAT_IMPL(left, right)
#define REALPERF_RECORDER (::realperf::Recorder::instance())
#define REALPERF_RECORDER_INIT(...) (REALPERF_RECORDER.init(__VA_ARGS__))
#define REALPERF_CHECKPOINT(...) (REALPERF_RECORDER.add(__VA_ARGS__))
#define REALPERF_EVENT(Event, ...) (REALPERF_RECORDER.addEvent<Event>(__VA_ARGS__))
#define REALPERF_START(...) (REALPERF_RECORDER.start(__VA_ARGS__))
#define REALPERF_END(...) (REALPERF_RECORDER.end(__VA_ARGS__))
#define REALPERF_COMMIT() (REALPERF_RECORDER.commit())
#define REALPERF_ROLLBACK() (REALPERF_RECORDER.rollback())
#define REALPERF_COMMIT_IF_HAS(category) (REALPERF_RECORDER.commitIfHas(category))
#define REALPERF_SCOPE(...) [[maybe_unused]] ::realperf::CheckPointScope REALPERF_DETAIL_CONCAT(realperf_scope_, __COUNTER__)(__VA_ARGS__)

}
