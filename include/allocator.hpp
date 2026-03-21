/**
 * @file allocator.hpp
 * @brief Allocator interface, OOM handling, and default allocator wiring.
 *
 * @details
 * Contracts and constraints:
 * - All sizes and alignments are expressed in bytes.
 * - Alignments must be powers of two.
 * - `try_allocate` / `try_reallocate` return `nullptr` on failure and never throw.
 * - `deallocate` is a no-op when given `nullptr`.
 * - `try_reallocate` preserves `min(old_size, new_size)` bytes and may move.
 * - OOM handling is opt-in: if no handler is installed, allocation failures
 *   return `nullptr` immediately.
 * - OOM handlers may request a retry. Retries are capped by `get_oom_retries()`.
 * - Implementations must be thread-safe with respect to their own internal state
 *   if shared across threads, but this interface does not impose a global lock.
 */

#pragma once

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <new>

#include "impl.hpp"
#include "typedefs.hpp"

namespace duck {

/// @brief Result requested by an out-of-memory handler.
enum class OOMHandlerAction : U8 { Fail, Retry };
/**
 * @brief Function pointer signature for OOM handlers.
 *
 * @param size Requested allocation size in bytes.
 * @param alignment Requested alignment in bytes (power of two).
 * @return `Retry` to attempt allocation again, `Fail` to stop.
 */
using OOMHandler = OOMHandlerAction (*)(USize size, USize alignment) noexcept;

namespace impl {
/// @brief Internal storage for the global OOM handler pointer.
inline std::atomic<OOMHandler>& get_oom_handler_slot() noexcept {
    static std::atomic<OOMHandler> slot { nullptr };
    return slot;
}

/// @brief Internal storage for the global OOM retry count.
inline std::atomic<U8>& get_oom_retries_slot() noexcept {
    static std::atomic<U8> slot { 2 };
    return slot;
}
}  // namespace impl

/// @brief Returns the current global OOM handler (may be `nullptr`).
inline OOMHandler get_oom_handler() noexcept {
    return impl::get_oom_handler_slot().load(std::memory_order_acquire);
}

/**
 * @brief Installs a new global OOM handler and returns the previous one.
 *
 * @param handler New handler function (may be `nullptr` to disable).
 */
inline OOMHandler set_oom_handler(OOMHandler handler) noexcept {
    return impl::get_oom_handler_slot().exchange(handler, std::memory_order_acq_rel);
}

/// @brief Returns the maximum number of retries for OOM handling.
inline U8 get_oom_retries() noexcept {
    return impl::get_oom_retries_slot().load(std::memory_order_acquire);
}

/**
 * @brief Sets the maximum number of retries for OOM handling.
 *
 * @param retries Maximum retry attempts after an OOM handler requests retry.
 */
inline U8 set_oom_retries(U8 retries) noexcept {
    return impl::get_oom_retries_slot().exchange(retries, std::memory_order_acq_rel);
}

/**
 * @brief Abstract allocator interface with OOM retry behavior.
 *
 * @details
 * Implementations must provide `impl_try_allocate` and `impl_deallocate`.
 * The base class provides a default `impl_try_reallocate` that allocates,
 * copies, and frees.
 */
class AllocatorI {
   public:
    /// @brief Virtual destructor for safe polymorphic deletion.
    virtual ~AllocatorI() = default;

    /**
     * @brief Attempts to allocate a block of memory.
     *
     * @param size Size in bytes. Must be non-zero.
     * @param alignment Alignment in bytes. Must be a power of two.
     * @return Pointer to allocated memory, or `nullptr` on failure.
     */
    [[nodiscard]] void* try_allocate(USize size, USize alignment) noexcept {
        assert(size != 0);
        assert(impl::is_valid_alignment(alignment));

        if (size == 0) [[unlikely]] {
            return nullptr;
        }

        if (!impl::is_valid_alignment(alignment)) [[unlikely]] {
            return nullptr;
        }

        U8 max_retries = get_oom_retries();
        for (U8 attempt = 0;; ++attempt) {
            if (void* pointer = impl_try_allocate(size, alignment)) return pointer;

            OOMHandler allocation_error_handler = get_oom_handler();

            if (allocation_error_handler == nullptr) return nullptr;
            if (attempt >= max_retries) return nullptr;
            if (allocation_error_handler(size, alignment) != OOMHandlerAction::Retry)
                return nullptr;
        }
    }

    /**
     * @brief Attempts to resize an existing allocation.
     *
     * @param pointer Original allocation (non-null).
     * @param old_size Size of the original allocation in bytes (non-zero).
     * @param new_size Requested new size in bytes (non-zero).
     * @param alignment Alignment in bytes. Must be a power of two.
     * @return Pointer to resized memory (may move), or `nullptr` on failure.
     */
    [[nodiscard]] void* try_reallocate(void* pointer, USize old_size, USize new_size,
                                       USize alignment) noexcept {
        assert(pointer);
        assert(old_size != 0);
        assert(new_size != 0);
        assert(impl::is_valid_alignment(alignment));

        if (!pointer) [[unlikely]] {
            return nullptr;
        }

        if (old_size == 0) [[unlikely]] {
            return nullptr;
        }

        if (new_size == 0) [[unlikely]] {
            return nullptr;
        }

        if (!impl::is_valid_alignment(alignment)) [[unlikely]] {
            return nullptr;
        }

        U8 max_retries = get_oom_retries();
        for (U8 attempt = 0;; ++attempt) {
            if (void* p = impl_try_reallocate(pointer, old_size, new_size, alignment)) {
                return p;
            }

            OOMHandler allocation_error_handler = get_oom_handler();

            if (allocation_error_handler == nullptr) return nullptr;
            if (attempt >= max_retries) return nullptr;
            if (allocation_error_handler(new_size, alignment) != OOMHandlerAction::Retry)
                return nullptr;
        }
    }

    /**
     * @brief Deallocates a previously allocated block.
     *
     * @param pointer Allocation to free (may be `nullptr`).
     * @param size Size of the allocation in bytes (non-zero when pointer is valid).
     * @param alignment Alignment in bytes. Must be a power of two.
     */
    void deallocate(void* pointer, USize size, USize alignment) noexcept {
        if (!pointer) return;

        assert(size != 0);
        assert(impl::is_valid_alignment(alignment));

        if (size == 0) [[unlikely]] {
            return;
        }

        if (!impl::is_valid_alignment(alignment)) [[unlikely]]
            return;

        impl_deallocate(pointer, size, alignment);
    }

   protected:
    /**
     * @brief Implementation hook for allocation attempts.
     *
     * @param size Size in bytes. Guaranteed non-zero by the public API.
     * @param alignment Alignment in bytes (power of two).
     * @return Pointer on success, or `nullptr` on failure.
     */
    virtual void* impl_try_allocate(USize size, USize alignment) noexcept = 0;

    /**
     * @brief Implementation hook for reallocation attempts.
     *
     * @details Default behavior allocates a new block, copies the data, and
     * deallocates the old block.
     */
    virtual void* impl_try_reallocate(void* pointer, USize old_size, USize new_size,
                                      USize alignment) noexcept {
        void* new_pointer = try_allocate(new_size, alignment);
        if (!new_pointer) return nullptr;

        Byte* dst = static_cast<Byte*>(new_pointer);
        Byte* src = static_cast<Byte*>(pointer);
        USize n   = std::min(old_size, new_size);

        std::memcpy(dst, src, n);
        deallocate(pointer, old_size, alignment);

        return dst;
    }

    /**
     * @brief Implementation hook for deallocation.
     *
     * @param pointer Allocation to free (non-null).
     * @param size Size in bytes (non-zero).
     * @param alignment Alignment in bytes (power of two).
     */
    virtual void impl_deallocate(void* pointer, USize size, USize alignment) noexcept = 0;
};

/// @brief Allocator that forwards to global `operator new/delete`.
class NewDeleteAllocator final : public AllocatorI {
   protected:
    /// @brief Allocate via the global nothrow `operator new`.
    void* impl_try_allocate(USize size, USize alignment) noexcept override {
        if (alignment <= alignof(std::max_align_t)) [[likely]] {
            return ::operator new(size, std::nothrow);
        }

        return ::operator new(size, std::align_val_t(alignment), std::nothrow);
    }

    /// @brief Deallocate via the sized global `operator delete`.
    void impl_deallocate(void* pointer, USize size, USize alignment) noexcept override {
        if (alignment <= alignof(std::max_align_t)) [[likely]] {
            ::operator delete(pointer, size);
        } else {
            ::operator delete(pointer, size, std::align_val_t(alignment));
        }
    }
};

/// @brief Allocator that forwards to `malloc`/`free` with `realloc` optimization.
class MallocAllocator final : public AllocatorI {
   protected:
    /// @brief Allocate via `malloc` or `aligned_alloc` for over-aligned requests.
    void* impl_try_allocate(USize size, USize alignment) noexcept override {
        if (alignment <= alignof(std::max_align_t)) [[likely]] {
            return std::malloc(size);
        }

        USize normalized  = impl::normalize_alignment(alignment);
        USize aligned_size = (size + normalized - 1) & ~(normalized - 1);

        return std::aligned_alloc(normalized, aligned_size);
    }

    /// @brief Reallocate via `realloc` when alignment allows it.
    void* impl_try_reallocate(void* pointer, USize old_size, USize new_size,
                              USize alignment) noexcept override {
        if (alignment <= alignof(std::max_align_t)) [[likely]] {
            (void)old_size;
            return std::realloc(pointer, new_size);
        }

        return AllocatorI::impl_try_reallocate(pointer, old_size, new_size, alignment);
    }

    /// @brief Deallocate via `free`.
    void impl_deallocate(void* pointer, USize size, USize alignment) noexcept override {
        (void)size;
        (void)alignment;
        std::free(pointer);
    }
};

/// @brief Returns the singleton instance of the `NewDeleteAllocator`.
inline AllocatorI* get_new_delete_allocator() noexcept {
    static AllocatorI* pointer = [] {
        return static_cast<AllocatorI*>(new NewDeleteAllocator {});
    }();

    return pointer;
}

/// @brief Returns the singleton instance of the `MallocAllocator`.
inline AllocatorI* get_new_malloc_allocator() noexcept {
    static AllocatorI* pointer = [] {
        return static_cast<AllocatorI*>(new MallocAllocator {});
    }();

    return pointer;
}

namespace impl {
/// @brief Internal storage for the global default allocator.
inline std::atomic<AllocatorI*>& get_default_allocator_slot() noexcept {
    static std::atomic<AllocatorI*> slot { get_new_delete_allocator() };
    return slot;
}
}  // namespace impl

/// @brief Returns the current global default allocator.
inline AllocatorI* get_default_allocator() noexcept {
    return impl::get_default_allocator_slot().load(std::memory_order_acquire);
}

/**
 * @brief Installs a new global default allocator and returns the previous one.
 *
 * @param allocator New allocator (must be non-null).
 */
inline AllocatorI* set_default_allocator(AllocatorI* allocator) noexcept {
    assert(allocator);
    if (!allocator) return get_default_allocator();

    return impl::get_default_allocator_slot().exchange(allocator,
                                                       std::memory_order_acq_rel);
}
}  // namespace duck
