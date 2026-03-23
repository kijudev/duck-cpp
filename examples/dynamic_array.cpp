#include "../include/dynamic_array.hpp"

#include <print>

int main() {
    duck::DynamicArray<U32> nums = { 1, 2, 3, 4 };

    for (U32 n : nums) {
        std::println("n -> {}", n);
    }

    return 0;
}
