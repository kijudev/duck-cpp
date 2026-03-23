#include <print>

#include "../include/dynamic_array.hpp"

int main() {
    duck::DynamicArray<U32> numbers = { 1, 2, 3, 4 };

    for (U32 n : numbers) {
        std::println("n -> {}", n);
    }

    return 0;
}
