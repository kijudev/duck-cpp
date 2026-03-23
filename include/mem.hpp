/**
 * @file mem.hpp
 * @brief Memory helpers.
 */

#pragma once

#include <cassert>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>

#include "allocator.hpp"
#include "typedefs.hpp"

namespace duck::mem {

template <typename T>
struct Storage {
    Allocator* allocator { get_default_allocator() };
    T* data { nullptr };
    USize size { 0 };
    USize capacity { 0 };

    inline static Storage empty() noexcept { return Storage {}; }
    inline static Storage with_alloc(Allocator* allocator) {
        return {
            .allocator = allocator,
            .data      = nullptr,
            .size      = 0,
            .capacity  = 0,
        };
    }
};

template <typename T>
inline void copy_raw_range(const T* src, USize sz, T* dst) noexcept {
    static_assert(std::is_trivially_copyable_v<T>, "@todo");
    assert(dst >= src + sz || src >= dst + sz);

    std::memcpy(static_cast<void*>(dst), static_cast<const void*>(src), sz * sizeof(T));
}

template <typename T>
inline void destroy_range(T* ptr, USize sz) noexcept {
    static_assert(std::is_nothrow_destructible_v<T>, "@todo");

    if (!ptr) return;
    if (sz == 0) return;

    if constexpr (!std::is_trivially_destructible_v<T>) {
        for (USize i = sz; i-- > 0;) {
            std::destroy_at(ptr + i);
        }
    }
}

template <typename T>
inline void copy_init_range(const T* src, USize sz, T* dst) noexcept {
    assert(src);
    assert(dst);
    assert(dst >= src + sz || src >= dst + sz);

    if (sz == 0) return;

    if constexpr (std::is_trivially_copyable_v<T>) {
        copy_raw_range(src, sz, dst);
    } else {
        static_assert(std::is_nothrow_copy_constructible_v<T>, "@todo");

        for (USize i = 0; i < sz; ++i) {
            std::construct_at(dst + i, src[i]);
        }
    }
}

template <typename T>
inline void copy_assign_range(const T* src, USize sz, T* dst) noexcept {
    assert(src);
    assert(dst);
    assert(dst >= src + sz || src >= dst + sz);

    if (sz == 0) return;

    if constexpr (std::is_trivially_copyable_v<T>) {
        copy_raw_range(src, sz, dst);
    } else {
        static_assert(std::is_nothrow_copy_assignable_v<T>, "@todo");

        for (USize i = 0; i < sz; ++i) {
            dst[i] = src[i];
        }
    }
}

template <typename T>
inline void move_init_range(T* src, USize sz, T* dst) noexcept {
    assert(src);
    assert(dst);
    assert(dst >= src + sz || src >= dst + sz);

    if (sz == 0) return;

    if constexpr (std::is_trivially_copyable_v<T>) {
        copy_raw_range(src, sz, dst);
    } else {
        static_assert(std::is_nothrow_move_constructible_v<T>, "@todo");

        for (USize i = 0; i < sz; ++i) {
            std::construct_at(dst + i, std::move(src[i]));
        }
    }
}

template <typename T>
inline void move_assign_range(T* src, USize sz, T* dst) noexcept {
    assert(src);
    assert(dst);
    assert(dst >= src + sz || src >= dst + sz);

    if (sz == 0) return;

    if constexpr (std::is_trivially_copyable_v<T>) {
        copy_raw_range(src, sz, dst);
    } else {
        static_assert(std::is_nothrow_move_assignable_v<T>, "@todo");

        for (USize i = 0; i < sz; ++i) {
            dst[i] = std::move(src[i]);
        }
    }
}

template <typename T>
inline void default_init_range(T* ptr, USize sz) noexcept {
    assert(ptr);

    static_assert(std::is_nothrow_default_constructible_v<T>);
    if constexpr (!std::is_trivially_default_constructible_v<T>) {
        for (USize i = 0; i < sz; ++i) std::construct_at(ptr + i);
    }
}

template <typename T>
inline void value_init_range(T* ptr, USize sz) noexcept {
    assert(ptr);

    if constexpr (std::is_trivially_default_constructible_v<T>) {
        std::memset(ptr, 0, sz * sizeof(T));
    } else {
        static_assert(std::is_nothrow_default_constructible_v<T>);
        for (USize i = 0; i < sz; ++i) std::construct_at(ptr + i);
    }
}

template <typename T>
inline void transfer_init_range(T* src, USize sz, T* dst) noexcept {
    assert(dst >= src + sz || src >= dst + sz);

    if constexpr (std::is_trivially_copyable_v<T>) {
        std::memcpy(dst, src, sz * sizeof(T));
    } else {
        static_assert(std::is_nothrow_move_constructible_v<T> &&
                      std::is_nothrow_destructible_v<T>);
        for (USize i = 0; i < sz; ++i) {
            std::construct_at(dst + i, std::move(src[i]));
            std::destroy_at(src + i);
        }
    }
}

template <typename T>
inline void transfer_assign_range(T* src, USize sz, T* dst) noexcept {
    assert(dst >= src + sz || src >= dst + sz);

    if constexpr (std::is_trivially_copyable_v<T>) {
        std::memcpy(dst, src, sz * sizeof(T));
    } else {
        static_assert(std::is_nothrow_move_assignable_v<T> &&
                      std::is_nothrow_destructible_v<T>);
        for (USize i = 0; i < sz; ++i) {
            dst[i] = std::move(src[i]);
            std::destroy_at(src + i);
        }
    }
}

template <typename T>
inline void shift_range_right(T* ptr, USize count, USize by) noexcept {
    assert(ptr);
    if (count == 0) return;

    if constexpr (std::is_trivially_copyable_v<T>) {
        std::memmove(ptr + by, ptr, count * sizeof(T));
    } else {
        static_assert(std::is_nothrow_move_constructible_v<T> &&
                      std::is_nothrow_destructible_v<T>);
        for (USize i = count; i-- > 0;) {
            std::construct_at(ptr + by + i, std::move(ptr[i]));
            std::destroy_at(ptr + i);
        }
    }
}

template <typename T>
inline void shift_range_left(T* ptr, USize count, USize by) noexcept {
    assert(ptr);
    if (count == 0) return;

    if constexpr (std::is_trivially_copyable_v<T>) {
        std::memmove(ptr, ptr + by, count * sizeof(T));
    } else {
        static_assert(std::is_nothrow_move_constructible_v<T> &&
                      std::is_nothrow_destructible_v<T>);
        for (USize i = 0; i < count; ++i) {
            std::construct_at(ptr + i, std::move(ptr[by + i]));
            std::destroy_at(ptr + by + i);
        }
    }
}
}  // namespace duck::mem
