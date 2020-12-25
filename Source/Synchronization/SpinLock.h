#pragma once

#include "SpinWait.h"

namespace neworld::synchronization {
    class spin_lock {
    public:
        void lock() noexcept {
            for (;;) {
                auto expect = false;
                if (m_atomic.compare_exchange_strong(expect, true, std::memory_order_acquire))
                    return;
                compete();
            }
        }

        void unlock() noexcept { m_atomic.store(false, std::memory_order_release); }

    private:
        void compete() const noexcept {
            spin_wait spinner{};
            while (m_atomic.load(std::memory_order_relaxed)) { spinner.spin_once(); }
        }

        alignas(64) std::atomic_bool m_atomic = {false};
    };
}