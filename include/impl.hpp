#pragma once

#include <algorithm>

#include "typedefs.hpp"

namespace duck::impl {

/**
 * @brief Internal alignment helpers used by the allocator layer.
 *
 * @details Constraints and expectations:
 * - Alignment values are treated as raw power-of-two requirements.
 * - A valid alignment is any non-zero power of two.
 * - No function here performs allocation; they are pure helpers.
 * - `normalize_alignment` never reduces alignment; it only raises it to at least
 *   `alignof(void*)` to satisfy typical allocation APIs.
 * - These helpers assume the caller has already validated inputs where required.
 */

/**
 * @brief Checks if a value is a power of two.
 *
 * @param n The value to test.
 * @return `true` if `n` is a non-zero power of two; otherwise `false`.
 *
 * @note This is a low-level helper with no side effects.
 */
inline bool is_power_of_2(USize n) noexcept { return n && ((n & (n - 1)) == 0); }

/**
 * @brief Validates that an alignment value is acceptable for the allocator.
 *
 * @param alignment The requested alignment in bytes.
 * @return `true` if the alignment is a non-zero power of two.
 *
 * @note This intentionally mirrors standard alignment requirements.
 */
inline bool is_valid_alignment(USize alignment) noexcept {
    return is_power_of_2(alignment);
}

/**
 * @brief Normalizes an alignment to be at least pointer-aligned.
 *
 * @param aligment The requested alignment in bytes.
 * @return The max of `aligment` and `alignof(void*)`.
 *
 * @note The spelling of `aligment` is preserved to avoid API churn.
 */
inline USize normalize_alignment(USize aligment) noexcept {
    return std::max(alignof(void*), aligment);
}
}  // namespace duck::impl
