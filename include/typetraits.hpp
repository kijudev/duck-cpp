#pragma once

#include <type_traits>

namespace duck::traits {
template <typename T>
struct is_trivially_relocatable : std::false_type {};

template <typename T>
inline constexpr bool is_trivially_relocatable_v = is_trivially_relocatable<T>::value;
}  // namespace duck::traits
