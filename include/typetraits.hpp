#pragma once

#include <type_traits>

namespace duck::traits {
template <typename T>
struct IsBitwiseRelocatableT : std::bool_constant<false> {};

template <typename T>
inline constexpr bool IsBitwiseRelocatableV = IsBitwiseRelocatableT<T>::value;

template <typename T>
concept BitwiseRelocatable = IsBitwiseRelocatableV<T>;
}  // namespace duck::traits
