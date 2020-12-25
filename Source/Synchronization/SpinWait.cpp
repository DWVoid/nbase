#include "SpinWait.h"
#include "Common/Timing.h"
#include <cmath>
#include <algorithm>

using namespace neworld;
using namespace neworld::synchronization;

namespace {
    // measured typically 37-46 on post-Skylake
    constexpr unsigned int min_normalized_yield_ns = 37;
    // approx. 900 cycles, measured 281 on pre-Skylake, 263 on post-Skylake
    constexpr unsigned int optimal_max_spin_duration_ns = 272;
    // measurement constants
    constexpr auto measure_duration = 10;
    constexpr auto ns_per_second = 1000 * 1000 * 1000;

    // Intel pre-Skylake processor: measured typically 14-17 cycles per yield
    // Intel post-Skylake processor: measured typically 125-150 cycles per yield
    double measure_yield_ns(std::int64_t tick_hz) noexcept {
        const auto ticks_to_measure = tick_hz / (1000 / measure_duration);
        std::int64_t start_tick, now_tick;
        query_performance_counter(&start_tick);
        const auto expected_end_tick = start_tick + ticks_to_measure;
        unsigned int counter = 0;
        // On some systems, querying the high performance counter has relatively significant overhead.
        // Do enough yields to mask the timing overhead. Assuming one yield has a delay of min_normalized_yield_ns,
        // 1000 yields would have a delay in the low microsecond range.
        do {
            for (auto i = 0; i < 1000; ++i) IDLE;
            counter += 1000;
            query_performance_counter(&now_tick);
        } while (now_tick < expected_end_tick);

        return std::max(1.0, double(now_tick - start_tick) * ns_per_second / (double(counter) * tick_hz));
    }

    unsigned int compute_processor_normalized_yield() noexcept {
        std::int64_t tick_hz;
        if (!query_performance_frequency(&tick_hz) || tick_hz < 1000 / measure_duration) {
            return 7;  // High precision clock not available or clock resolution is too low, use default value
        }
        const auto yield_time = measure_yield_ns(tick_hz);
        // Calculate the number of yields required to span the duration of a normalized yield.
        // Since nsPerYield is at least 1, this value is naturally limited to min_normalized_yield_ns.
        const auto normalization_multiplier = std::max(1l, std::lround(min_normalized_yield_ns / yield_time));
        // Calculate the maximum number of yields that would be optimal for a late spin iteration.
        // Typically, we would not want to spend excessive amounts of time (thousands of cycles) doing
        // only YieldProcessor, as SwitchToThread/Sleep would do a better job of allowing other work to run.
        return std::max(1l, std::lround(optimal_max_spin_duration_ns / (normalization_multiplier * yield_time)));
    }
}

const bool spin_wait::single_processor = std::thread::hardware_concurrency() == 1;
const unsigned int spin_wait::optimal_max_spin_iterations = compute_processor_normalized_yield();