#pragma once

#include <cstdint>

namespace neworld::memory {
    static constexpr uintptr_t memory_block_size = 4ull << 20ull;

    uintptr_t fetch_block() noexcept;

    void return_block(uintptr_t block) noexcept;
}