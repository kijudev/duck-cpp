#include "allocator.hpp"

#include <print>

int main() {
    duck::AllocatorI* heap_alloc = duck::get_default_allocator();

    int* a = static_cast<int*>(heap_alloc->try_allocate(sizeof(int), alignof(int)));

    *a = 42;
    std::println("a: {}", *a);

    heap_alloc->deallocate(a, sizeof(int), alignof(int));
}
