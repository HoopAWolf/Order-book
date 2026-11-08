#ifndef A8B6D59F_DD63_407E_824F_188253616659
#define A8B6D59F_DD63_407E_824F_188253616659

#include <algorithm>
#include <chrono>
#include <cstdint>
#if defined(__x86_64__)
#include <x86intrin.h>
#define OB_HAS_TSC 1
#endif

namespace ob
{
    /**
     * @brief Reads the current high-resolution timer value.
     * @return Timestamp in CPU ticks on supported x86 targets, otherwise nanoseconds.
     */
    inline uint64_t ticksNow()
    {
#ifdef OB_HAS_TSC
        _mm_lfence();
        const uint64_t t = __rdtsc();
        _mm_lfence();
        return t;
#else
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
    }

    /**
     * @brief Estimates timer ticks per nanosecond.
     * @return Estimated number of timer ticks per nanosecond.
     */
    inline double ticksPerNs()
    {
        using clk = std::chrono::steady_clock;
        const auto c0 = clk::now();
        const uint64_t t0 = ticksNow();
        while (clk::now() - c0 < std::chrono::milliseconds(200))
        {
        }

        const uint64_t t1 = ticksNow();
        const auto c1 = clk::now();
        return double(t1 - t0) / std::chrono::duration<double, std::nano>(c1 - c0).count();
    }

    /**
     * @brief Estimates the minimum overhead of reading the timer twice.
     * @return Smallest observed tick difference across the measurements.
     */
    inline uint64_t timerOverheadTicks()
    {
        uint64_t best = ~0ull;
        for (int i = 0; i < 10000; ++i)
        {
            const uint64_t a = ticksNow();
            const uint64_t b = ticksNow();
            best = std::min(best, b - a);
        }

        return best;
    }
}

#endif /* A8B6D59F_DD63_407E_824F_188253616659 */
