#pragma once

#include <algorithm>

#include "typedefs.hpp"

namespace duck::impl {
inline bool is_power_of_2(USize n) noexcept { return n && ((n & (n - 1)) == 0); }

inline bool is_valid_aligment(USize alignment) noexcept { return is_power_of_2(alignment); }

inline USize normalize_alignment(USize aligment) noexcept {
    return std::max(alignof(void*), aligment);
}
}  // namespace duck::impl
