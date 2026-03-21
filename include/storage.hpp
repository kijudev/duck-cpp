#pragma once

#include "allocator.hpp"

namespace duck::impl {
struct StorageBytes {
    AllocatorI* allocator { get_default_allocator() };
    Byte* data { nullptr };
    USize size { 0 };
    USize capacity { 0 };

    inline static StorageBytes empty() noexcept { return StorageBytes {}; }
};

template <typename T>
struct StorageT {
    AllocatorI* allocator { get_default_allocator() };
    T* data { nullptr };
    USize size { 0 };
    USize capacity { 0 };

    inline static StorageT empty() noexcept { return StorageT {}; }
    inline static StorageT with_alloc(AllocatorI* allocator) {
        return {
            .allocator = allocator,
            .data      = nullptr,
            .size      = 0,
            .capacity  = 0,
        };
    }
};
}  // namespace duck::impl
