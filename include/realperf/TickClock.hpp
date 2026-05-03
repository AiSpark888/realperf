#pragma once

#include "realperf/TickTimer.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>

namespace realperf {

class TickClock {
public:
    using Tick = TickTimer::Tick;
    using Clock = std::chrono::system_clock;
    using Duration = std::chrono::nanoseconds;
    using TimePoint = std::chrono::time_point<Clock, Duration>;

    TickClock() noexcept
    {
        const Sample sample = takeSample();
        anchorTick_.store(sample.tick, std::memory_order_relaxed);
        anchorNanoseconds_.store(sample.nanoseconds, std::memory_order_relaxed);
        lastTick_.store(sample.tick, std::memory_order_relaxed);
        lastNanoseconds_.store(sample.nanoseconds, std::memory_order_relaxed);
        sampleCount_.store(1u, std::memory_order_relaxed);
    }

    static Tick now() noexcept
    {
        return TickTimer::now();
    }

    void sync() noexcept
    {
        const Sample sample = takeSample();
        const std::uint64_t sampleCount = sampleCount_.load(std::memory_order_acquire);

        if (sampleCount > 0u) {
            const Tick previousTick = lastTick_.load(std::memory_order_relaxed);
            const std::int64_t previousNanoseconds =
                lastNanoseconds_.load(std::memory_order_relaxed);
            const Tick elapsedTicks = sample.tick - previousTick;
            const std::int64_t elapsedNanoseconds =
                sample.nanoseconds - previousNanoseconds;

            if (elapsedTicks > 0u && elapsedNanoseconds > 0) {
                scaleTicks_.store(elapsedTicks, std::memory_order_release);
                scaleNanoseconds_.store(elapsedNanoseconds, std::memory_order_release);
            }
        }

        lastTick_.store(sample.tick, std::memory_order_release);
        lastNanoseconds_.store(sample.nanoseconds, std::memory_order_release);
        anchorTick_.store(sample.tick, std::memory_order_release);
        anchorNanoseconds_.store(sample.nanoseconds, std::memory_order_release);
        sampleCount_.store(sampleCount + 1u, std::memory_order_release);
    }

    [[nodiscard]] std::int64_t toNanoseconds(Tick tick) const
    {
        const Tick anchorTick = anchorTick_.load(std::memory_order_acquire);
        const std::int64_t anchorNanoseconds =
            anchorNanoseconds_.load(std::memory_order_acquire);
        const std::uint64_t scaleTicks = scaleTicks_.load(std::memory_order_acquire);
        const std::int64_t scaleNanoseconds =
            scaleNanoseconds_.load(std::memory_order_acquire);

        const std::int64_t offset =
            scaleTicks > 0u && scaleNanoseconds > 0
                ? scaledNanoseconds(tickDelta(tick, anchorTick),
                    scaleNanoseconds,
                    scaleTicks)
                : fallbackScaledNanoseconds(tickDelta(tick, anchorTick));

        return saturatingAdd(anchorNanoseconds, offset);
    }

    [[nodiscard]] TimePoint toTimePoint(Tick tick) const
    {
        return TimePoint(Duration(toNanoseconds(tick)));
    }

    [[nodiscard]] std::int64_t nowNanoseconds() const
    {
        return toNanoseconds(now());
    }

    [[nodiscard]] TimePoint nowTimePoint() const
    {
        return toTimePoint(now());
    }

    [[nodiscard]] std::uint64_t syncCount() const noexcept
    {
        return sampleCount_.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool calibrated() const noexcept
    {
        return scaleTicks_.load(std::memory_order_acquire) > 0u;
    }

private:
    struct Sample {
        Tick tick;
        std::int64_t nanoseconds;
    };

    static Sample takeSample() noexcept
    {
        const Tick tickBefore = TickTimer::now();
        const auto now = Clock::now();
        const Tick tickAfter = TickTimer::now();

        return Sample {
            tickBefore + ((tickAfter - tickBefore) / 2u),
            std::chrono::duration_cast<Duration>(now.time_since_epoch()).count(),
        };
    }

    static __int128 tickDelta(Tick tick, Tick anchorTick) noexcept
    {
        const Tick unsignedDelta = tick - anchorTick;
        if (unsignedDelta <= static_cast<Tick>(std::numeric_limits<std::int64_t>::max())) {
            return static_cast<__int128>(unsignedDelta);
        }

        return -static_cast<__int128>(anchorTick - tick);
    }

    static std::int64_t scaledNanoseconds(
        __int128 ticks,
        std::int64_t scaleNanoseconds,
        std::uint64_t scaleTicks) noexcept
    {
        const __int128 value =
            (ticks * static_cast<__int128>(scaleNanoseconds))
            / static_cast<__int128>(scaleTicks);
        return clampToInt64(value);
    }

    static std::int64_t fallbackScaledNanoseconds(__int128 ticks)
    {
        const double ticksPerSecond = TickTimer::ticksPerSecond();
        if (ticksPerSecond <= 0.0) {
            return 0;
        }

        const long double nanoseconds =
            static_cast<long double>(ticks) * 1'000'000'000.0L
            / static_cast<long double>(ticksPerSecond);

        if (nanoseconds >= static_cast<long double>(std::numeric_limits<std::int64_t>::max())) {
            return std::numeric_limits<std::int64_t>::max();
        }
        if (nanoseconds <= static_cast<long double>(std::numeric_limits<std::int64_t>::min())) {
            return std::numeric_limits<std::int64_t>::min();
        }

        return static_cast<std::int64_t>(nanoseconds);
    }

    static std::int64_t saturatingAdd(std::int64_t lhs, std::int64_t rhs) noexcept
    {
        return clampToInt64(static_cast<__int128>(lhs) + static_cast<__int128>(rhs));
    }

    static std::int64_t clampToInt64(__int128 value) noexcept
    {
        if (value > static_cast<__int128>(std::numeric_limits<std::int64_t>::max())) {
            return std::numeric_limits<std::int64_t>::max();
        }
        if (value < static_cast<__int128>(std::numeric_limits<std::int64_t>::min())) {
            return std::numeric_limits<std::int64_t>::min();
        }

        return static_cast<std::int64_t>(value);
    }

    std::atomic<Tick> anchorTick_ {0u};
    std::atomic<std::int64_t> anchorNanoseconds_ {0};
    std::atomic<Tick> lastTick_ {0u};
    std::atomic<std::int64_t> lastNanoseconds_ {0};
    std::atomic<std::uint64_t> scaleTicks_ {0u};
    std::atomic<std::int64_t> scaleNanoseconds_ {0};
    std::atomic<std::uint64_t> sampleCount_ {0u};
};

} // namespace realperf
