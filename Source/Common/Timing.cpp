#include "Timing.h"

#if __has_include(<Windows.h>)
#include "System/WindowsSlim.h"
#define HAS_WIN32_CLOCK 1
#elif __has_include(<mach/mach_time.h>)
#define HAVE_MACH_ABSOLUTE_TIME 1
#include <mach/mach_time.h>
#elif __has_include(<time.h>)
#include <ctime>
#if defined(CLOCK_MONOTONIC)
#define HAVE_CLOCK_MONOTONIC 1
#endif
#endif

namespace {
#ifndef HAS_WIN32_CLOCK
    constexpr int64_t tcc_seconds_to_nanoseconds = 1000000000;
#endif

#if HAVE_MACH_ABSOLUTE_TIME
    mach_timebase_info_data_t time_base_init() {
        mach_timebase_info_data_t val{};
        mach_timebase_info(&val);
    }

    mach_timebase_info_data_t timer_base = time_base_init();
#endif
}

bool neworld::query_performance_counter(std::int64_t* out_value) noexcept {
#if HAVE_MACH_ABSOLUTE_TIME
    return (*out_value = mach_absolute_time(), true);
#elif HAVE_CLOCK_MONOTONIC
    struct timespec ts{};
    const auto result = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (result) return false;
    return (*out_value = tcc_seconds_to_nanoseconds * ts.tv_sec + ts.tv_nsec, true);
#elif HAS_WIN32_CLOCK
    return static_cast<bool>(QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(out_value)));
#else
#error "NO PROPPER CLOCK SUPPORT"
#endif
}

bool neworld::query_performance_frequency(std::int64_t* out_value) noexcept {
#if HAVE_MACH_ABSOLUTE_TIME
    if (timer_base.denom == 0) return false;
    return (*out_value = (tcc_seconds_to_nanoseconds * timer_base.denom) / timer_base.numer, true);
#elif HAVE_CLOCK_MONOTONIC
    return (*out_value = tcc_seconds_to_nanoseconds, true);
#elif HAS_WIN32_CLOCK
    return static_cast<bool>(QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(out_value)));
#else
#error "NO PROPPER CLOCK SUPPORT"
#endif
}