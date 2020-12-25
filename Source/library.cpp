#include <iostream>
#include <queue>
#include "Memory/Blocks.h"

const auto mul = 4;

int main() {
    std::cout << std::hex;
    std::queue<uintptr_t> stack {};
    for (int i = 0; i < 10 * mul; ++i) stack.push(neworld::memory::fetch_block());
    std::cout << "P1" << std::endl;
    for (int i = 0; i < 5 * mul; ++i) {
        neworld::memory::return_block(stack.front());
        stack.pop();
    }
    std::cout << "P2" << std::endl;
    for (int i = 0; i < 10 * mul; ++i) stack.push(neworld::memory::fetch_block());
    std::cout << "P3" << std::endl;
    for (int i = 0; i < 15 * mul; ++i) {
        neworld::memory::return_block(stack.front());
        stack.pop();
    }
    std::cout << "P4" << std::endl;
}
