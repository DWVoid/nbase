#pragma once

#include <thread>
#include <limits>

#if __has_include(<x86intrin.h>)
#include <x86intrin.h>
#define IDLE _mm_pause()
#elif __has_include(<intrin.h>)

#include <intrin.h>

#define IDLE _mm_pause()
#else
#define IDLE asm("nop")
#endif

namespace neworld::synchronization {
    struct spin_wait {
    public:
        [[nodiscard]] unsigned int count() const noexcept { return m_count; }

        [[nodiscard]] bool yield_next_spin() const noexcept { return m_count >= yield_threshold || single_processor; }

        void spin_once() noexcept { spin_core(default_sleep1_threshold); }

        void spin_once(unsigned int threshold) noexcept {
            if (threshold >= 0 && threshold < yield_threshold) threshold = yield_threshold;
            spin_core(threshold);
        }

        void reset() noexcept { m_count = 0; }

    private:
        // When to switch over to a true yield.
        static constexpr unsigned int yield_threshold = 10;
        // After how many yields should we Sleep(0)?
        static constexpr unsigned int sleep0_freq = 5;
        // After how many yields should we Sleep(1) frequently?
        static constexpr unsigned int default_sleep1_threshold = 20;
        static const bool single_processor;
        static const unsigned int optimal_max_spin_iterations;
        unsigned int m_count = 0;

        void spin_core(const unsigned int threshold) noexcept {
            if ((m_count >= yield_threshold
                 && ((m_count >= threshold && threshold >= 0) || (m_count - yield_threshold) % 2 == 0))
                || single_processor) {
                if (m_count >= threshold && threshold >= 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                } else {
                    const auto cur_yields = m_count >= yield_threshold ? (m_count - yield_threshold) / 2 : m_count;
                    if ((cur_yields % sleep0_freq) == (sleep0_freq - 1)) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(0));
                    } else std::this_thread::yield();
                }
            } else {
                auto n = optimal_max_spin_iterations;
                if (m_count <= 30 && (1u << m_count) < n) n = 1u << m_count;
                spin(n);
            }

            // Finally, increment our spin counter.
            m_count = (m_count == std::numeric_limits<int>::max() ? yield_threshold : m_count + 1);
        }

        static void spin(const unsigned int iterations) noexcept { for (auto i = 0u; i < iterations; ++i) IDLE; }
    };
}