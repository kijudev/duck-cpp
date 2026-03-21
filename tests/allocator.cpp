#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>

#include "allocator.hpp"
#include "typedefs.hpp"

namespace {
constexpr USize k_small  = 64;
constexpr USize k_grow   = 128;
constexpr USize k_shrink = 32;

constexpr Byte k_pattern_a { 0xAB };
constexpr Byte k_pattern_b { 0xCD };

bool is_aligned(const void* pointer, USize alignment) noexcept {
    std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(pointer);
    return (addr % alignment) == 0;
}

void fill_pattern(Byte* pointer, USize size, Byte value) noexcept {
    std::memset(pointer, static_cast<S32>(std::to_integer<U8>(value)), size);
}

bool matches_pattern(const Byte* pointer, USize size, Byte value) noexcept {
    for (USize i = 0; i < size; ++i) {
        if (pointer[i] != value) return false;
    }

    return true;
}

void run_allocator_contracts(duck::AllocatorI& allocator, USize alignment) {
    // Base case: deallocating a null pointer must be a no-op.
    // Size 0 is allowed here because the implementation returns before asserting.
    CHECK_NOTHROW(allocator.deallocate(nullptr, 0, alignment));

    // Contract: allocate must succeed for small sizes and return aligned memory.
    void* raw = allocator.try_allocate(k_small, alignment);
    REQUIRE(raw != nullptr);
    CHECK(is_aligned(raw, alignment));

    Byte* bytes = static_cast<Byte*>(raw);
    fill_pattern(bytes, k_small, k_pattern_a);

    // Contract: reallocate must preserve min(old_size, new_size) bytes.
    void* grown = allocator.try_reallocate(raw, k_small, k_grow, alignment);
    REQUIRE(grown != nullptr);

    Byte* grown_bytes = static_cast<Byte*>(grown);
    CHECK(matches_pattern(grown_bytes, k_small, k_pattern_a));
    CHECK(is_aligned(grown, alignment));

    // Base case: shrinking still preserves the prefix.
    fill_pattern(grown_bytes, k_grow, k_pattern_b);

    void* shrunk = allocator.try_reallocate(grown, k_grow, k_shrink, alignment);
    REQUIRE(shrunk != nullptr);

    Byte* shrunk_bytes = static_cast<Byte*>(shrunk);
    CHECK(matches_pattern(shrunk_bytes, k_shrink, k_pattern_b));
    CHECK(is_aligned(shrunk, alignment));

    allocator.deallocate(shrunk, k_shrink, alignment);
}
}  // namespace

TEST_CASE("NewDeleteAllocator contracts") {
    duck::NewDeleteAllocator allocator;

    SUBCASE("default alignment") {
        run_allocator_contracts(allocator, alignof(std::max_align_t));
    }

    SUBCASE("over-aligned") {
        run_allocator_contracts(allocator, alignof(std::max_align_t) * 2);
    }
}

TEST_CASE("MallocAllocator contracts") {
    duck::MallocAllocator allocator;

    SUBCASE("default alignment") {
        run_allocator_contracts(allocator, alignof(std::max_align_t));
    }

    SUBCASE("over-aligned") {
        run_allocator_contracts(allocator, alignof(std::max_align_t) * 2);
    }
}
