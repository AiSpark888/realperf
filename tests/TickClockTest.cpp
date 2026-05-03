#include "realperf/TickClock.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <thread>

namespace {

std::int64_t system_nanoseconds()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        realperf::TickClock::Clock::now().time_since_epoch())
        .count();
}

} // namespace

TEST_CASE("TickClock calibrates from sync samples")
{
    realperf::TickClock clock;

    CHECK(clock.syncCount() == 1u);
    CHECK_FALSE(clock.calibrated());

    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    clock.sync();

    CHECK(clock.syncCount() == 2u);
    CHECK(clock.calibrated());
}

TEST_CASE("TickClock converts ticks to wall-clock nanoseconds")
{
    realperf::TickClock clock;

    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    clock.sync();

    const auto tick = realperf::TickClock::now();
    const std::int64_t converted = clock.toNanoseconds(tick);
    const std::int64_t wall_clock = system_nanoseconds();

    CHECK(converted <= wall_clock + 20'000'000);
    CHECK(converted >= wall_clock - 20'000'000);
}

TEST_CASE("TickClock preserves tick ordering")
{
    realperf::TickClock clock;

    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    clock.sync();

    const auto first_tick = realperf::TickClock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const auto second_tick = realperf::TickClock::now();

    CHECK(clock.toNanoseconds(second_tick) > clock.toNanoseconds(first_tick));
}
