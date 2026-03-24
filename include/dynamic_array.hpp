#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <utility>

#include "allocator.hpp"
#include "mem.hpp"
#include "slice.hpp"

namespace duck {

template <typename T>
class DynamicArray {
   private:
    mem::Storage<T> m_storage { mem::Storage<T>::empty() };

   public:
    using iterator        = T*;
    using const_iterator  = const T*;
    using value_type      = T;
    using size_type       = USize;
    using difference_type = std::ptrdiff_t;
    using pointer         = T*;
    using const_pointer   = const T*;
    using reference       = T&;
    using const_reference = const T&;

    // ------------------------------------------------------------------------
    // Construction / Destruction
    // -------------------------------------------------------------------------

    DynamicArray() noexcept = default;

    DynamicArray(std::initializer_list<T> list) noexcept {
        static_assert(std::is_nothrow_copy_constructible_v<T>);
        do_grow_if_needed(list.size());

        for (const T& item : list) {
            emplace_back(item);
        }
    }

    DynamicArray(const DynamicArray& other) noexcept { do_copy_from(other); }
    DynamicArray(DynamicArray&& other) noexcept { do_move_from(std::move(other)); }

    DynamicArray& operator=(const DynamicArray& other) noexcept {
        if (this == &other) return *this;

        do_assign_copy(other);
        return *this;
    }

    DynamicArray& operator=(DynamicArray&& other) noexcept {
        if (this == &other) return *this;

        do_destroy_all();
        do_free_buffer();
        do_move_from(std::move(other));

        return *this;
    }

    ~DynamicArray() noexcept {
        do_destroy_all();
        do_free_buffer();
    }

    // -------------------------------------------------------------------------
    // Sane Constructors
    // -------------------------------------------------------------------------

    [[nodiscard]] static DynamicArray with_allocator(Allocator* allocator) noexcept {
        DynamicArray darr;
        darr.m_storage.allocator = allocator;

        return darr;
    }

    [[nodiscard]] static DynamicArray with_capacity(USize capacity) noexcept {
        return with_capacity(get_default_allocator(), capacity);
    }

    [[nodiscard]] static DynamicArray with_capacity(Allocator* allocator,
                                                    USize capacity) noexcept {
        DynamicArray darr;
        darr.m_storage.allocator = allocator;
        darr.do_reserve(capacity);

        return darr;
    }

    [[nodiscard]] static DynamicArray with_size(USize size) noexcept {
        return with_size(get_default_allocator(), size);
    }

    [[nodiscard]] static DynamicArray with_size(Allocator* allocator, USize size) noexcept {
        DynamicArray arr;
        arr.m_storage.allocator = allocator;
        arr.do_reserve(size);

        mem::default_init_range(arr.m_storage.data, size);
        arr.m_storage.size = size;

        return arr;
    }

    [[nodiscard]] static DynamicArray filled_with(USize size) noexcept {
        return filled_with(size);
    }

    [[nodiscard]] static DynamicArray filled_with(Allocator* allocator, USize size,
                                                  const T& fill) noexcept {
        static_assert(std::is_nothrow_copy_constructible_v<T>);

        DynamicArray arr;
        arr.m_storage.allocator = allocator;
        arr.do_reserve(size);

        for (USize i = 0; i < size; ++i) {
            std::construct_at(arr.m_storage.data + i, fill);
        }

        arr.m_storage.size = size;

        return arr;
    }

    // -------------------------------------------------------------------------
    // Iterators
    // -------------------------------------------------------------------------

    T* begin() noexcept { return m_storage.data; }
    T* end() noexcept { return m_storage.data + m_storage.size; }
    const T* begin() const noexcept { return m_storage.data; }
    const T* end() const noexcept { return m_storage.data + m_storage.size; }
    const T* cbegin() const noexcept { return m_storage.data; }
    const T* cend() const noexcept { return m_storage.data + m_storage.size; }

    // -------------------------------------------------------------------------
    // Element Access
    // -------------------------------------------------------------------------

    T& operator[](USize idx) noexcept {
        assert(idx < m_storage.size);
        return m_storage.data[idx];
    }

    const T& operator[](USize idx) const noexcept {
        assert(idx < m_storage.size);
        return m_storage.data[idx];
    }

    T& front() noexcept {
        assert(!is_empty());
        return m_storage.data[0];
    }

    const T& front() const noexcept {
        assert(!is_empty());
        return m_storage.data[0];
    }

    T& back() noexcept {
        assert(!is_empty());
        return m_storage.data[m_storage.size - 1];
    }

    const T& back() const noexcept {
        assert(!is_empty());
        return m_storage.data[m_storage.size - 1];
    }

    T* data() noexcept { return m_storage.data; }
    const T* data() const noexcept { return m_storage.data; }

    Slice<T> slice() const& noexcept { return Slice<T>(m_storage.data, m_storage.size); }
    Slice<T> slice() const&& noexcept = delete;

    Slice<T> slice(USize from, USize to) const& noexcept { return slice().slice(from, to); }
    Slice<T> slice(USize, USize) const&& = delete;

    Slice<T> slice_from(USize from) const& noexcept { return slice().slice_from(from); }
    Slice<T> slice_from(USize) const&& = delete;

    Slice<T> slice_to(USize to) const& noexcept { return slice().slice_to(to); }
    Slice<T> slice_to(USize) const&& = delete;

    // -------------------------------------------------------------------------
    // Capacity
    // -------------------------------------------------------------------------

    USize size() const noexcept { return m_storage.size; }
    USize capacity() const noexcept { return m_storage.capacity; }
    bool is_empty() const noexcept { return m_storage.size == 0; }
    bool is_full() const noexcept { return m_storage.size == m_storage.capacity; }

    Allocator* allocator() const noexcept { return m_storage.allocator; }

    void reserve(USize new_capacity) noexcept {
        if (new_capacity > m_storage.capacity) do_reserve(new_capacity);
    }

    void shrink_to_fit() noexcept {
        if (m_storage.size == m_storage.capacity) return;
        if (m_storage.size == 0) {
            do_free_buffer();
            return;
        }

        do_reallocate(m_storage.size);
    }

    // -------------------------------------------------------------------------
    // Modifiers
    // -------------------------------------------------------------------------

    void push_back(const T& value) noexcept { emplace_back(value); }
    void push_back(T&& value) noexcept { emplace_back(std::move(value)); }

    template <typename... Args>
    T& emplace_back(Args&&... args) noexcept {
        static_assert(std::is_nothrow_constructible_v<T, Args...>);

        do_grow_if_full();
        T* ptr =
            std::construct_at(m_storage.data + m_storage.size, std::forward<Args>(args)...);
        ++m_storage.size;

        return *ptr;
    }

    void pop_back() noexcept {
        assert(!is_empty());

        --m_storage.size;
        std::destroy_at(m_storage.data + m_storage.size);
    }

    // O(1) unordered remove — swaps with back then pops
    void remove_swap(USize idx) noexcept {
        assert(idx < m_storage.size);

        if (idx != m_storage.size - 1) {
            m_storage.data[idx] = std::move(m_storage.data[m_storage.size - 1]);
        }

        pop_back();
    }

    // O(n) ordered remove - preserves element order
    void remove_shift(USize idx) noexcept {
        assert(idx < m_storage.size);

        mem::shift_range_left(m_storage.data + idx, m_storage.size - idx - 1, 1);
        --m_storage.size;

        std::destroy_at(m_storage.data + m_storage.size);
    }

    void insert(USize idx, const T& value) noexcept {
        static_assert(std::is_nothrow_copy_constructible_v<T>);
        assert(idx <= m_storage.size);

        do_grow_if_full();
        mem::shift_range_right(m_storage.data + idx, m_storage.size - idx, 1);
        std::construct_at(m_storage.data + idx, value);

        ++m_storage.size;
    }

    void insert(USize idx, T&& value) noexcept {
        static_assert(std::is_nothrow_move_constructible_v<T>);
        assert(idx <= m_storage.size);

        do_grow_if_full();
        mem::shift_range_right(m_storage.data + idx, m_storage.size - idx, 1);
        std::construct_at(m_storage.data + idx, std::move(value));

        ++m_storage.size;
    }

    void clear() noexcept {
        do_destroy_all();
        m_storage.size = 0;
    }

    void shrink(USize new_size) noexcept {
        mem::destroy_range(m_storage.data + new_size, m_storage.size - new_size);
        m_storage.size = new_size;
    }

    void grow_default(USize new_size) noexcept {
        do_grow_if_needed(new_size);
        mem::default_init_range(m_storage.data + m_storage.size, new_size - m_storage.size);

        m_storage.size = new_size;
    }

    void grow_with(USize new_size, const T& fill) noexcept {
        do_grow_if_needed(new_size);

        for (USize i = m_storage.size; i < new_size; ++i) {
            std::construct_at(m_storage.data + i, fill);
        }

        m_storage.size = new_size;
    }

   private:
    // -------------------------------------------------------------------------
    // Helpers, implementation.
    // -------------------------------------------------------------------------

    USize do_next_capacity(USize current) const noexcept {
        if (current == 0) return 8;
        return current + current / 2;  // 1.5x growth
    }

    void do_reserve(USize new_capacity) noexcept {
        assert(new_capacity > m_storage.capacity);

        if (!m_storage.data) {
            m_storage.data = static_cast<T*>(
                m_storage.allocator->allocate(new_capacity * sizeof(T), alignof(T)));
            m_storage.capacity = new_capacity;
            return;
        }

        do_reallocate(new_capacity);
    }

    void do_reallocate(USize new_capacity) noexcept {
        assert(new_capacity >= m_storage.size);

        // @todo Implement is_trivially_relocatble
        if constexpr (std::is_trivially_copyable<T>::value) {
            m_storage.data = static_cast<T*>(m_storage.allocator->reallocate(
                m_storage.data, m_storage.capacity * sizeof(T), new_capacity * sizeof(T),
                alignof(T)));
        } else {
            T* new_data = static_cast<T*>(
                m_storage.allocator->allocate(new_capacity * sizeof(T), alignof(T)));
            mem::transfer_init_range(m_storage.data, m_storage.size, new_data);
            m_storage.allocator->deallocate(m_storage.data, m_storage.capacity * sizeof(T),
                                            alignof(T));
            m_storage.data = new_data;
        }

        m_storage.capacity = new_capacity;
    }

    void do_grow_if_full() noexcept {
        if (is_full()) {
            do_reserve(do_next_capacity(m_storage.capacity));
        }
    }

    void do_grow_if_needed(USize required_size) noexcept {
        if (required_size > m_storage.capacity) {
            do_reserve(std::max(do_next_capacity(m_storage.capacity), required_size));
        }
    }

    void do_destroy_all() noexcept { mem::destroy_range(m_storage.data, m_storage.size); }

    void do_free_buffer() noexcept {
        if (!m_storage.data) return;

        m_storage.allocator->deallocate(m_storage.data, m_storage.capacity * sizeof(T),
                                        alignof(T));
        m_storage.data     = nullptr;
        m_storage.capacity = 0;
    }

    void do_copy_from(const DynamicArray& other) noexcept {
        static_assert(std::is_nothrow_copy_constructible_v<T>);

        m_storage.allocator = other.m_storage.allocator;
        if (other.m_storage.size == 0) return;

        do_reserve(other.m_storage.size);
        mem::copy_init_range(other.m_storage.data, other.m_storage.size, m_storage.data);

        m_storage.size = other.m_storage.size;
    }

    void do_assign_copy(const DynamicArray& other) noexcept {
        static_assert(std::is_nothrow_copy_constructible_v<T> &&
                      std::is_nothrow_copy_assignable_v<T>);

        const USize overlap = std::min(m_storage.size, other.m_storage.size);

        if (overlap > 0) {
            mem::copy_assign_range(other.m_storage.data, overlap, m_storage.data);
        }

        if (other.m_storage.size > m_storage.size) {
            do_grow_if_needed(other.m_storage.size);
            mem::copy_init_range(other.m_storage.data + overlap,
                                 other.m_storage.size - overlap, m_storage.data + overlap);
        } else if (other.m_storage.size < m_storage.size) {
            mem::destroy_range(m_storage.data + other.m_storage.size,
                               m_storage.size - other.m_storage.size);
        }

        m_storage.size = other.m_storage.size;
        // @note Allocator is not propagated on copy assign, only on copy construction.
    }

    void do_move_from(DynamicArray&& other) noexcept {
        m_storage       = other.m_storage;
        other.m_storage = mem::Storage<T>::empty();
    }
};

}  // namespace duck
