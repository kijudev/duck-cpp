#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

#include "storage.hpp"

namespace duck {
template <typename T>
class DynamicArrayT {
   private:
    impl::StorageT<T> m_storage { impl::StorageT<T>::empty() };

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

    DynamicArrayT() noexcept = default;

    explicit DynamicArrayT(AllocatorI* allocator) noexcept
        : m_storage(impl::StorageT<T>::with_alloc(allocator)) {}

    DynamicArrayT(const DynamicArrayT& other) noexcept { impl_copy_from(other); }

    DynamicArrayT(DynamicArrayT&& other) noexcept { impl_move_from(std::move(other)); }

    DynamicArrayT& operator=(const DynamicArrayT& other) noexcept {
        if (this == &other) return *this;

        return *this;
    }

    DynamicArrayT& operator=(DynamicArrayT&& other) noexcept {
        if (this == &other) return *this;

        return *this;
    }

    ~DynamicArrayT() noexcept {}

   public:
    T* begin() noexcept { return m_storage.data; }
    T* end() noexcept { return m_storage.data + m_storage.size; }
    const T* begin() const noexcept { return m_storage.data; }
    const T* end() const noexcept { return m_storage.data + m_storage.size; }
    const T* cbegin() const noexcept { return m_storage.data; }
    const T* cend() const noexcept { return m_storage.data + m_storage.size; }

    T& front() noexcept { return m_storage.data[0]; }
    const T& front() const noexcept { return m_storage.data[0]; }
    T& back() noexcept { return m_storage.data[m_storage.size - 1]; }
    const T& back() const noexcept { return m_storage.data[m_storage.size - 1]; }

    T* data() noexcept { return m_storage.data; }
    const T* data() const noexcept { return m_storage.data; }

    USize size() const noexcept { return m_storage.size; }
    USize capacity() const noexcept { return m_storage.capacity; }
    bool is_empty() const noexcept { return m_storage.size == 0; }
    bool is_full() const noexcept { return m_storage.size == m_storage.capacity; }

    AllocatorI* allocator() const noexcept { return m_storage.allocator; }

    T& operator[](USize idx) noexcept { return m_storage.data[idx]; }
    const T& operator[](USize idx) const noexcept { return m_storage.data[idx]; }

   private:
};
}  // namespace duck
