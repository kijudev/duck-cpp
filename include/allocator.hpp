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

enum class AllocationErrorHandlerAction : U8 { Fail, Retry };
using AllocationErrorHandler = AllocationErrorHandlerAction (*)(USize size,
                                                                USize alignment) noexcept;

namespace impl::global {
inline std::atomic<AllocationErrorHandler> allocation_error_handler(nullptr);
}  // namespace impl::global

inline AllocationErrorHandler get_allocation_error_handler() noexcept {
    return impl::global::allocation_error_handler.load(std::memory_order_acquire);
}

inline AllocationErrorHandler set_allocation_error_handler(
    AllocationErrorHandler handler) noexcept {
    return impl::global::allocation_error_handler.exchange(handler,
                                                           std::memory_order_relaxed);
}

class AllocatorI {
   public:
    virtual ~AllocatorI() = default;

    [[nodiscard]] void* try_allocate(USize size, USize alignment) noexcept {
        assert(size != 0);
        assert(impl::is_valid_aligment(alignment));

        if (size == 0) return nullptr;
        if (!impl::is_valid_aligment(alignment)) return nullptr;

        for (U8 attempt = 0;; ++attempt) {
            if (void* pointer = impl_try_allocate(size, alignment)) return pointer;

            AllocationErrorHandler allocation_error_handler =
                get_allocation_error_handler();

            if (allocation_error_handler == nullptr) return nullptr;
            if (attempt >= 1) return nullptr;
            if (allocation_error_handler(size, alignment) !=
                AllocationErrorHandlerAction::Retry)
                return nullptr;
        }
    }

    [[nodiscard]] void* try_reallocate(void* pointer, USize old_size, USize new_size,
                                       USize alignment) noexcept {
        assert(pointer);
        assert(old_size != 0);
        assert(new_size != 0);
        assert(impl::is_valid_aligment(alignment));

        if (!pointer) return nullptr;
        if (old_size == 0) return nullptr;
        if (new_size == 0) return nullptr;
        if (!impl::is_valid_aligment(alignment)) return nullptr;

        return impl_try_reallocate(pointer, old_size, new_size, alignment);
    }

    void deallocate(void* pointer, USize size, USize alignment) noexcept {
        assert(pointer);
        assert(size != 0);
        assert(impl::is_valid_aligment(alignment));

        if (!pointer) return;
        if (size == 0) return;
        if (!impl::is_valid_aligment(alignment)) return;

        impl_deallocate(pointer, size, alignment);
    }

   protected:
    /// @note: Both size and alignment are already validated.
    virtual void* impl_try_allocate(USize size, USize aligment) noexcept = 0;

    /// @note: Pointer, size and alignment are already validated.
    /// @note: Deafult implementation of try_reallocate falls back to allocate - copy -
    /// deallocate strategy.
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

    /// @note: Pointer, size and alignment are already validated.
    virtual void impl_deallocate(void* pointer, USize size, USize aligment) noexcept = 0;
};

class NewDeleteAllocator final : public AllocatorI {
   protected:
    void* impl_try_allocate(USize size, USize alignment) noexcept override {
        if (alignment <= alignof(std::max_align_t)) [[likely]] {
            return ::operator new(size, std::nothrow);
        }

        return ::operator new(size, std::align_val_t(alignment), std::nothrow);
    }

    void* impl_try_reallocate(void* pointer, USize old_size, USize new_size,
                              USize alignment) noexcept override {
        // Standard std::realloc work only for small, common alignments.
        if (alignment <= alignof(std::max_align_t)) [[likely]] {
            return std::realloc(pointer, new_size);
        }

        void* new_pointer = try_allocate(new_size, alignment);
        if (!new_pointer) return nullptr;

        Byte* dst = static_cast<Byte*>(new_pointer);
        Byte* src = static_cast<Byte*>(pointer);
        USize n   = std::min(old_size, new_size);

        std::memcpy(dst, src, n);
        deallocate(pointer, old_size, alignment);

        return dst;
    };

    void impl_deallocate(void* pointer, USize /* size */,
                         USize alignment) noexcept override {
        if (alignment <= alignof(std::max_align_t)) [[likely]] {
            ::operator delete(pointer, std::nothrow);
        } else {
            ::operator delete(pointer, std::align_val_t(alignment), std::nothrow);
        }
    }
};

namespace impl::global {
inline std::atomic<AllocatorI*> default_allocator = new NewDeleteAllocator;
}

inline AllocatorI* get_default_allocator() noexcept {
    return impl::global::default_allocator.load(std::memory_order_acquire);
}

inline AllocatorI* get_default_allocator(AllocatorI* allocator) noexcept {
    return impl::global::default_allocator.exchange(allocator, std::memory_order_relaxed);
}

}  // namespace duck
