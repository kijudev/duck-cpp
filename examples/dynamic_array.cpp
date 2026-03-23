#include "../include/dynamic_array.hpp"

#include <print>

int main() {
    duck::DynamicArray<U32> a = { 1, 2, 3, 4 };

    for (U32 n : a) {
        std::println("a -> {}", n);
    }

    std::println("--------");

    auto b = a.slice(1, 2);

    for (U32 n : b) {
        std::println("b -> {}", n);
    }

    return 0;
}
