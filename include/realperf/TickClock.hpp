#pragma once

#include "realperf/TickTimer.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

namespace realperf {

// convert TSC ticks to nanoseconds using periodic synchronization with the system clock
class TickClock {
public:
    using Clock = std::chrono::system_clock;
    using Duration = std::chrono::nanoseconds;
    using EpochTime = std::chrono::nanoseconds;

    struct alignas(16) SyncPoint
    {
        void atomicStore(Tick tick, EpochTime epochTime) noexcept
        {
            // store tick and epochTime together atomically using a 128-bit store
            __m128i value = _mm_set_epi64x(epochTime.count(), tick);
            _mm_store_si128((__m128i*) this, value);
        }

        void atomicStore(SyncPoint syncPoint) noexcept
        {
            atomicStore(syncPoint.tick, syncPoint.epochTime);
        }

        SyncPoint atomicLoad() const noexcept
        {
            // load tick and epochTime together atomically using a 128-bit load
            __m128i value = _mm_load_si128((const __m128i*) this);
            Tick tick = _mm_extract_epi64(value, 0);
            EpochTime epochTime(_mm_extract_epi64(value, 1));
            return SyncPoint{tick, epochTime};
        }

        Tick tick;
        EpochTime epochTime;
    };
    static_assert(sizeof(SyncPoint) == 16, "SyncPoint size must be 16 bytes");
    static_assert(alignof(SyncPoint) == 16, "SyncPoint alignment must be 16 bytes");

    TickClock() noexcept
    {
        const SyncPoint sample = takeSample();
        anchor_.atomicStore(sample);
        last_.atomicStore(sample);
        sampleCount_.store(1u, std::memory_order_relaxed);
    }

    static Tick now() noexcept
    {
        return TickTimer::now();
    }

    void sync() noexcept
    {
        const SyncPoint sample = takeSample();
        const std::uint64_t sampleCount = sampleCount_.load(std::memory_order_acquire);

        if (sampleCount > 0u) {
            const SyncPoint previous = last_.atomicLoad();
            const Tick elapsedTicks = sample.tick - previous.tick;
            const std::int64_t elapsedNanoseconds =
                nanoseconds(sample) - nanoseconds(previous);

            if (elapsedTicks > 0u && elapsedNanoseconds > 0) {
                scaleTicks_.store(elapsedTicks, std::memory_order_release);
                scaleNanoseconds_.store(elapsedNanoseconds, std::memory_order_release);
            }
        }

        last_.atomicStore(sample);
        anchor_.atomicStore(sample);
        sampleCount_.store(sampleCount + 1u, std::memory_order_release);
    }

    [[nodiscard]] std::int64_t toNanoseconds(Tick tick) const
    {
        const SyncPoint anchor = anchor_.atomicLoad();
        const std::uint64_t scaleTicks = scaleTicks_.load(std::memory_order_acquire);
        const std::int64_t scaleNanoseconds =
            scaleNanoseconds_.load(std::memory_order_acquire);

        const std::int64_t offset =
            scaleTicks > 0u && scaleNanoseconds > 0
                ? scaledNanoseconds(tickDelta(tick, anchor.tick),
                    scaleNanoseconds,
                    scaleTicks)
                : fallbackScaledNanoseconds(tickDelta(tick, anchor.tick));

        return saturatingAdd(nanoseconds(anchor), offset);
    }

    [[nodiscard]] EpochTime toEpochTime(Tick tick) const
    {
        return EpochTime(Duration(toNanoseconds(tick)));
    }

    [[nodiscard]] std::int64_t nowNanoseconds() const
    {
        return toNanoseconds(now());
    }

    [[nodiscard]] EpochTime nowEpochTime() const
    {
        return toEpochTime(now());
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
    static SyncPoint takeSample() noexcept
    {
        const Tick tickBefore = TickTimer::now();
        const auto now = Clock::now();
        const Tick tickAfter = TickTimer::now();

        return SyncPoint {
            tickBefore + ((tickAfter - tickBefore) / 2u),
            std::chrono::duration_cast<Duration>(now.time_since_epoch()),
        };
    }

    static std::int64_t nanoseconds(SyncPoint syncPoint) noexcept
    {
        return syncPoint.epochTime.count();
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

    SyncPoint anchor_;
    SyncPoint last_;
    std::atomic<std::uint64_t> scaleTicks_ {0u};
    std::atomic<std::int64_t> scaleNanoseconds_ {0};
    std::atomic<std::uint64_t> sampleCount_ {0u};
};

} // namespace realperf
