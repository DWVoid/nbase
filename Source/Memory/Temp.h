#pragma once

#include <memory_resource>

namespace neworld::memory {
    std::pmr::memory_resource& temp_resource() noexcept;
}