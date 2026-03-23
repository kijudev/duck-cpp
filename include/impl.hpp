#pragma once

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>

#include "typedefs.hpp"

namespace duck::impl {

/**
 * @brief Internal alignment helpers used by the allocator layer.
 */

/**
 * @brief Checks if a value is a power of two.
 *
 * @param n The value to test.
 * @return `true` if `n` is a non-zero power of two; otherwise `false`.
 *
 * @note This is a low-level helper with no side effects.
 */
inline bool is_pow2(USize n) noexcept { return n && ((n & (n - 1)) == 0); }

/**
 * @brief Rounds a value up to the nearest power of two.
 *
 * @param n The value to round up.
 * @return The smallest power of two greater than or equal to `n`.
 *         Returns `1` if `n` is `0`.
 *
 * @note Uses `std::bit_width` from `<bit>` (C++20).
 */
inline USize round_up_pow2(USize n) noexcept {
    if (n == 0) return 1;
    if (is_pow2(n)) return n;

    return USize { 1 } << std::bit_width(n - 1);
}

/**
 * @brief Validates that an alignment value is acceptable for the allocator.
 *
 * @param alignment The requested alignment in bytes.
 * @return `true` if the alignment is a non-zero power of two.
 *
 * @note This intentionally mirrors standard alignment requirements.
 */
inline bool is_valid_alignment(USize alignment) noexcept { return is_pow2(alignment); }

/**
 * @brief Checks if an aligment value is overaligned.
 *
 * @param alignment The requested alignment in bytes.
 * @return `true` if the alignment is overaligned.
 */
inline bool is_overaligned(USize alignment) noexcept {
    return alignment >= sizeof(std::max_align_t);
}

/**
 * @brief Normalizes an alignment to be at least pointer-aligned.
 *
 * @param alignment Requested alignment in bytes.
 * @return The max of `alignment` and `alignof(void*)`.
 *
 * @pre `alignment` must be a non-zero power of two (validated by caller).
 */
inline USize normalize_alignment(USize alignment) noexcept {
    assert(is_pow2(alignment));
    return std::max(alignof(void*), alignment);
}

/**
 * @brief Rounds `size` up to the nearest multiple of `alignment`.
 *
 * @param size Size in bytes to round up.
 * @param alignment Alignment in bytes.
 * @return The smallest multiple of `alignment` greater than or equal to `size`.
 *
 * @pre `alignment` must be a non-zero power of two.
 * @note Uses bitmask arithmetic.
 */
inline USize round_up_to_multiple_of(USize size, USize alignment) noexcept {
    assert(is_pow2(alignment));
    return (size + alignment - 1) & ~(alignment - 1);
}
}  // namespace duck::impl
