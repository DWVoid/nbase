#pragma once

#include <cstdint>

namespace neworld {
    bool query_performance_counter(std::int64_t* out_value) noexcept;

    bool query_performance_frequency(std::int64_t* out_value) noexcept;
}