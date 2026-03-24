#pragma once

#include <algorithm>
#include <cassert>

#include "typedefs.hpp"

namespace duck {
template <typename T>
class Slice {
   private:
    T* m_data { nullptr };
    USize m_size { 0 };

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

    Slice() = delete;

    Slice(T* data, USize size) noexcept : m_data(data), m_size(size) {}

    Slice(const Slice& other) noexcept { do_copy_from(other); }
    Slice(Slice&& other) noexcept { do_move_from(std::move(other)); }

    Slice& operator=(const Slice& other) noexcept {
        do_copy_from(other);
        return *this;
    }

    Slice& operator=(Slice&& other) noexcept {
        do_move_from(other);
        return *this;
    }

    // ------------------------------------------------------------------------
    // Iterators
    // -------------------------------------------------------------------------

    const T* begin() const noexcept { return m_data; }
    const T* end() const noexcept { return m_data + m_size; }
    const T* cbegin() const noexcept { return m_data; }
    const T* cend() const noexcept { return m_data + m_size; }

    // ------------------------------------------------------------------------
    // Operations
    // -------------------------------------------------------------------------

    USize size() const noexcept { return m_size; }

    const T* data() const noexcept { return m_data; }

    bool is_empty() const noexcept { return m_size == 0; }

    Slice slice() const noexcept { return Slice(m_data, m_size); }
    Slice slice(USize from, USize to) const noexcept {
        assert(from < m_size);
        assert(to < m_size);
        assert(from <= to);

        return Slice(m_data + from, to - from + 1);
    }

    Slice slice_from(USize from) const noexcept {
        assert(from < m_size);
        return Slice(m_data + from, m_size - from);
    }

    Slice slice_to(USize to) const noexcept {
        assert(to < m_size);
        return Slice(m_data, to + 1);
    }

    // ------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

   private:
    void do_copy_from(const Slice& other) noexcept {
        m_data = other.m_data;
        m_size = other.m_size;
    }

    void do_move_from(Slice&& other) noexcept {
        m_data = other.m_data;
        m_size = other.m_size;
    }
};
}  // namespace duck
