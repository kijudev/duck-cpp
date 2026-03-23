#include <print>

#include "../include/allocator.hpp"

int main() {
    duck::Allocator* alloc = duck::get_new_delete_allocator();
    int* ptr               = static_cast<int*>(alloc->allocate(sizeof(int), alignof(int)));

    *ptr = 42;
    std::println("value: {}", *ptr);

    alloc->deallocate(ptr, sizeof(int), alignof(int));
}
