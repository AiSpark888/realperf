#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
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

struct Recorder
{

    Recorder() = default;

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
        end[1].tick_ = 123456789ull;
        if (begin[1].tick_ != 123456789ull)
        {
            throw std::runtime_error("Recorder must be initialized with a double mapped ring buffer");
        }
        ::memset(begin, 0, capacity_ * sizeof(CheckPoint));
    }

    Tick totalTicks() const
    {
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

private:

    CheckPoint * start_ = nullptr;
    std::uint16_t count_ = 0;
    std::uint16_t categories_ = 0;
    std::uint16_t minCount_ = 4;
    std::uint16_t mustHaveCategories_ = 0;
    CheckPoint * end_ = nullptr;
    std::size_t capacity_ = 0;
};

#define REALPERF_RECORDER (::realperf::Recorder::instance())
#define REALPERF_RECORDER_INIT(...) (REALPERF_RECORDER.init(__VA_ARGS__))
#define REALPERF_RECORD(...) (REALPERF_RECORDER.add(__VA_ARGS__))
#define REALPERF_RECORD_START(...) (REALPERF_RECORDER.start(__VA_ARGS__))
#define REALPERF_RECORD_END(...) (REALPERF_RECORDER.end(__VA_ARGS__))
#define REALPERF_RECORD_COMMIT() (REALPERF_RECORDER.commit())
#define REALPERF_RECORD_ROLLBACK() (REALPERF_RECORDER.rollback())
#define REALPERF_RECORD_COMMIT_IF_HAS(category) (REALPERF_RECORDER.commitIfHas(category))

}
