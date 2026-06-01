#include "realperf/CheckPoint.hpp"
#include "realperf/RingBuffer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include <unistd.h>

using realperf::LiteralString;

namespace {

struct TestEvent {
    TestEvent(std::uint32_t id, double value)
        : id_(id)
        , value_(value)
    {
    }

    std::uint32_t id_;
    double value_;
};

std::size_t checkpointCapacity()
{
    const long value = ::sysconf(_SC_PAGESIZE);
    REQUIRE(value > 0);
    return static_cast<std::size_t>(value) / sizeof(realperf::CheckPoint);
}

} // namespace

TEST_CASE("Recorder requires double-mapped checkpoint storage")
{
    std::array<realperf::CheckPoint, 10> storage {};
    realperf::Recorder recorder;

    CHECK_THROWS_AS(
        recorder.init(storage.data(), storage.data() + 8u),
        std::runtime_error);
}

TEST_CASE("Recorder default constructor uses dummy checkpoint storage")
{
    realperf::Recorder recorder;

    recorder.add(10u, "default first");
    recorder.add(15u, "default second");

    CHECK(recorder.has(realperf::Category::CAT_Default));
    CHECK(recorder.totalTicks() == 5u);

    recorder.commit();

    CHECK_FALSE(recorder.has(realperf::Category::CAT_Default));
}

TEST_CASE("Recorder adds checkpoints and tracks categories")
{
    realperf::RingBuffer<realperf::CheckPoint> buffer(checkpointCapacity());
    realperf::Recorder recorder;
    recorder.init(buffer.data(), buffer.data() + buffer.capacity(), 1u);

    recorder.add(10u, "first", realperf::CheckPoint::Type::CP_Start, realperf::Category::CAT_MD);
    recorder.add(25u, "second", realperf::CheckPoint::Type::CP_Normal, realperf::Category::CAT_Order);
    recorder.add(40u, "third", realperf::CheckPoint::Type::CP_End, realperf::Category::CAT_Strategy);

    CHECK(recorder.has(realperf::Category::CAT_MD));
    CHECK(recorder.has(realperf::Category::CAT_Order));
    CHECK(recorder.has(realperf::Category::CAT_Strategy));
    CHECK_FALSE(recorder.has(realperf::Category::CAT_Default));
    CHECK(recorder.totalTicks() == 30u);

    recorder.commit();

    CHECK(buffer.data()[0].where_ == LiteralString("first"));
    CHECK(buffer.data()[0].type_ == realperf::CheckPoint::Type::CP_Start);
    CHECK(buffer.data()[0].category_ == realperf::Category::CAT_MD);
    CHECK(buffer.data()[0].tick_ == 10u);

    CHECK(buffer.data()[1].where_ == LiteralString("second"));
    CHECK(buffer.data()[1].type_ == realperf::CheckPoint::Type::CP_Normal);
    CHECK(buffer.data()[1].category_ == realperf::Category::CAT_Order);
    CHECK(buffer.data()[1].tick_ == 25u);

    CHECK(buffer.data()[2].where_ == LiteralString("third"));
    CHECK(buffer.data()[2].type_ == realperf::CheckPoint::Type::CP_End);
    CHECK(buffer.data()[2].category_ == realperf::Category::CAT_Strategy);
    CHECK(buffer.data()[2].tick_ == 40u);
}

TEST_CASE("Recorder start and end overloads accept categories")
{
    realperf::RingBuffer<realperf::CheckPoint> buffer(checkpointCapacity());
    realperf::Recorder recorder;
    recorder.init(buffer.data(), buffer.data() + buffer.capacity(), 1u);

    recorder.start("category start", realperf::Category::CAT_MD, 10u);
    recorder.end("category end", realperf::Category::CAT_Order, 20u);

    CHECK(buffer.data()[0].where_ == LiteralString("category start"));
    CHECK(buffer.data()[0].type_ == realperf::CheckPoint::Type::CP_Start);
    CHECK(buffer.data()[0].category_ == realperf::Category::CAT_MD);
    CHECK(buffer.data()[0].tick_ == 10u);

    CHECK(buffer.data()[1].where_ == LiteralString("category end"));
    CHECK(buffer.data()[1].type_ == realperf::CheckPoint::Type::CP_End);
    CHECK(buffer.data()[1].category_ == realperf::Category::CAT_Order);
    CHECK(buffer.data()[1].tick_ == 20u);
}

TEST_CASE("Recorder rollback discards pending checkpoints")
{
    realperf::RingBuffer<realperf::CheckPoint> buffer(checkpointCapacity());
    realperf::Recorder recorder;
    recorder.init(buffer.data(), buffer.data() + buffer.capacity(), 1u);

    recorder.add(10u, "discarded", realperf::CheckPoint::Type::CP_Normal, realperf::Category::CAT_MD);
    REQUIRE(recorder.has(realperf::Category::CAT_MD));

    recorder.rollback();

    CHECK_FALSE(recorder.has(realperf::Category::CAT_MD));

    recorder.add(20u, "kept", realperf::CheckPoint::Type::CP_Normal, realperf::Category::CAT_Order);
    recorder.commit();

    CHECK(buffer.data()[0].where_ == LiteralString("kept"));
    CHECK(buffer.data()[0].category_ == realperf::Category::CAT_Order);
    CHECK(buffer.data()[0].tick_ == 20u);
}

TEST_CASE("Recorder commitIfHas commits matching categories and rolls back misses")
{
    realperf::RingBuffer<realperf::CheckPoint> buffer(checkpointCapacity());
    realperf::Recorder recorder;
    recorder.init(buffer.data(), buffer.data() + buffer.capacity(), 1u);

    recorder.add(10u, "first", realperf::CheckPoint::Type::CP_Normal, realperf::Category::CAT_MD);
    recorder.commitIfHas(realperf::Category::CAT_Order);

    recorder.add(20u, "second", realperf::CheckPoint::Type::CP_Normal, realperf::Category::CAT_Order);
    recorder.commitIfHas(realperf::Category::CAT_Order);

    recorder.add(30u, "third", realperf::CheckPoint::Type::CP_Normal, realperf::Category::CAT_Strategy);
    recorder.commit();

    CHECK(buffer.data()[0].where_ == LiteralString("second"));
    CHECK(buffer.data()[0].category_ == realperf::Category::CAT_Order);
    CHECK(buffer.data()[1].where_ == LiteralString("third"));
    CHECK(buffer.data()[1].category_ == realperf::Category::CAT_Strategy);
}

TEST_CASE("Recorder does not advance committed storage below the minimum count")
{
    realperf::RingBuffer<realperf::CheckPoint> buffer(checkpointCapacity());
    realperf::Recorder recorder;
    recorder.init(buffer.data(), buffer.data() + buffer.capacity(), 3u);

    recorder.add(10u, "too short 1");
    recorder.add(20u, "too short 2");
    recorder.commit();

    recorder.add(30u, "kept 1");
    recorder.add(40u, "kept 2");
    recorder.add(50u, "kept 3");
    recorder.commit();

    CHECK(buffer.data()[0].where_ == LiteralString("kept 1"));
    CHECK(buffer.data()[1].where_ == LiteralString("kept 2"));
    CHECK(buffer.data()[2].where_ == LiteralString("kept 3"));
}

TEST_CASE("Recorder commit wraps at the end of the mirrored buffer")
{
    realperf::RingBuffer<realperf::CheckPoint> buffer(checkpointCapacity());
    realperf::Recorder recorder;
    recorder.init(buffer.data(), buffer.data() + buffer.capacity(), 1u);


    for (std::size_t index = 0; index < buffer.capacity(); ++index) {
        recorder.add(static_cast<realperf::Tick>(index), "initial");
        recorder.commit();
    }

    recorder.add(999u, "wrapped");
    recorder.commit();

    CHECK(buffer.data()[0].where_ == LiteralString("wrapped"));
    CHECK(buffer.data()[0].tick_ == 999u);
    CHECK(buffer.data()[1].where_ == LiteralString("initial"));
    CHECK(buffer.data()[1].tick_ == 1u);
}

TEST_CASE("Recorder macros use the thread-local recorder instance")
{
    realperf::RingBuffer<realperf::CheckPoint> buffer(checkpointCapacity());
    REALPERF_RECORDER_INIT(buffer.data(), buffer.data() + buffer.capacity(), 1u);

    REALPERF_START("macro start", 100u);
    REALPERF_CHECKPOINT(125u, "macro normal", realperf::CheckPoint::Type::CP_Normal, realperf::Category::CAT_Order);
    CHECK(REALPERF_RECORDER.has(realperf::Category::CAT_Order));
    CHECK(REALPERF_RECORDER.totalTicks() == 25u);

    REALPERF_END("macro end", 150u);

    CHECK(buffer.data()[0].where_ == LiteralString("macro start"));
    CHECK(buffer.data()[0].type_ == realperf::CheckPoint::Type::CP_Start);
    CHECK(buffer.data()[0].tick_ == 100u);
    CHECK(buffer.data()[1].where_ == LiteralString("macro normal"));
    CHECK(buffer.data()[1].category_ == realperf::Category::CAT_Order);
    CHECK(buffer.data()[1].tick_ == 125u);
    CHECK(buffer.data()[2].where_ == LiteralString("macro end"));
    CHECK(buffer.data()[2].type_ == realperf::CheckPoint::Type::CP_End);
    CHECK(buffer.data()[2].tick_ == 150u);

    REALPERF_CHECKPOINT(175u, "macro rollback", realperf::CheckPoint::Type::CP_Normal, realperf::Category::CAT_MD);
    REALPERF_ROLLBACK();
    CHECK_FALSE(REALPERF_RECORDER.has(realperf::Category::CAT_MD));
}

TEST_CASE("CheckPointScope records scope start and end checkpoints")
{
    realperf::RingBuffer<realperf::CheckPoint> buffer(checkpointCapacity());
    realperf::Recorder recorder;
    recorder.init(buffer.data(), buffer.data() + buffer.capacity(), 1u);

    {
        realperf::CheckPointScope scope(recorder, "direct scope", realperf::Category::CAT_Strategy);
    }

    recorder.commit();

    CHECK(buffer.data()[0].where_ == LiteralString("direct scope"));
    CHECK(buffer.data()[0].type_ == realperf::CheckPoint::Type::CP_ScopeStart);
    CHECK(buffer.data()[0].category_ == realperf::Category::CAT_Strategy);
    CHECK(buffer.data()[1].where_ == LiteralString("direct scope"));
    CHECK(buffer.data()[1].type_ == realperf::CheckPoint::Type::CP_ScopeEnd);
    CHECK(buffer.data()[1].category_ == realperf::Category::CAT_Strategy);
    CHECK(buffer.data()[0].tick_ <= buffer.data()[1].tick_);
}

TEST_CASE("REALPERF_SCOPE records scope checkpoints on the thread-local recorder")
{
    realperf::RingBuffer<realperf::CheckPoint> buffer(checkpointCapacity());
    REALPERF_RECORDER_INIT(buffer.data(), buffer.data() + buffer.capacity(), 1u);

    {
        REALPERF_SCOPE("macro scope", realperf::Category::CAT_MD);
    }

    REALPERF_COMMIT();

    CHECK(buffer.data()[0].where_ == LiteralString("macro scope"));
    CHECK(buffer.data()[0].type_ == realperf::CheckPoint::Type::CP_ScopeStart);
    CHECK(buffer.data()[0].category_ == realperf::Category::CAT_MD);
    CHECK(buffer.data()[1].where_ == LiteralString("macro scope"));
    CHECK(buffer.data()[1].type_ == realperf::CheckPoint::Type::CP_ScopeEnd);
    CHECK(buffer.data()[1].category_ == realperf::Category::CAT_MD);
}

TEST_CASE("REALPERF_EVENT records typed event checkpoints")
{
    realperf::RingBuffer<realperf::CheckPoint> buffer(checkpointCapacity());
    REALPERF_RECORDER_INIT(buffer.data(), buffer.data() + buffer.capacity(), 1u);

    REALPERF_EVENT(TestEvent, 123u, "macro event", 42u, 9.75);
    REALPERF_COMMIT();

    const auto& eventCheckpoint = *reinterpret_cast<const realperf::EventCheckPoint<TestEvent>*>(buffer.data());

    CHECK(eventCheckpoint.type_ == realperf::CheckPoint::Type::CP_Event);
    CHECK(eventCheckpoint.category_ == realperf::Category::CAT_Event);
    CHECK(eventCheckpoint.reserved_ == sizeof(TestEvent));
    CHECK(eventCheckpoint.where_ == LiteralString("macro event"));
    CHECK(eventCheckpoint.tick_ == 123u);
    CHECK(eventCheckpoint.id_ == 42u);
    CHECK(eventCheckpoint.value_ == 9.75);
}
