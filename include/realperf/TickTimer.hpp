#pragma once

#include <chrono>
#include <cstdint>
#include <thread>
#include <x86intrin.h>

namespace realperf {

class TickTimer {
public:
    using Tick = std::uint64_t;

    TickTimer() noexcept
        : last_(now())
    {
    }

    explicit TickTimer(Tick tsc) noexcept
        : last_(tsc)
    {
    }

    static Tick now() noexcept
    {
        return __rdtsc();
    }

    static double ticksPerSecond()
    {
        static const double frequency = measureFrequency();
        return frequency;
    }

    Tick checkpoint(Tick tsc) noexcept
    {
        const Tick elapsed = tsc - last_;
        last_ = tsc;
        return elapsed;
    }

    Tick checkpoint() noexcept
    {
        return checkpoint(now());
    }

    Tick elapsed(Tick tsc) const noexcept
    {
        return tsc - last_;
    }

    Tick elapsed() const noexcept
    {
        return elapsed(now());
    }

    void reset(Tick tsc) noexcept
    {
        last_ = tsc;
    }

    void reset() noexcept
    {
        reset(now());
    }

private:
    static double measureFrequency()
    {
        using clock = std::chrono::steady_clock;
        using seconds = std::chrono::duration<double>;

        const auto startTime = clock::now();
        const Tick startTicks = now();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        const Tick endTicks = now();
        const auto endTime = clock::now();

        const double elapsedSeconds = seconds(endTime - startTime).count();
        return elapsedSeconds > 0.0
            ? static_cast<double>(endTicks - startTicks) / elapsedSeconds
            : 0.0;
    }

    Tick last_;
};

} // namespace realperf
