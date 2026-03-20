#pragma once

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <new>

#include "impl.hpp"
#include "typedefs.hpp"

namespace duck {

enum class OOMHandlerAction : U8 { Fail, Retry };
using OOMHandler = OOMHandlerAction (*)(USize size, USize alignment) noexcept;

namespace impl {
inline std::atomic<OOMHandler>& get_oom_handler_slot() noexcept {
    static std::atomic<OOMHandler> slot { nullptr };
    return slot;
}

inline std::atomic<U8>& get_oom_retries_slot() noexcept {
    static std::atomic<U8> slot { 2 };
    return slot;
}
}  // namespace impl

inline OOMHandler get_oom_handler() noexcept {
    return impl::get_oom_handler_slot().load(std::memory_order_acquire);
}

inline OOMHandler set_oom_handler(OOMHandler handler) noexcept {
    return impl::get_oom_handler_slot().exchange(handler, std::memory_order_acq_rel);
}

inline U8 get_oom_retries() noexcept {
    return impl::get_oom_retries_slot().load(std::memory_order_acquire);
}

inline U8 set_oom_retries(U8 retries) noexcept {
    return impl::get_oom_retries_slot().exchange(retries, std::memory_order_acq_rel);
}

class AllocatorI {
   public:
    virtual ~AllocatorI() = default;

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
    virtual void* impl_try_allocate(USize size, USize alignment) noexcept = 0;

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

    virtual void impl_deallocate(void* pointer, USize size, USize alignment) noexcept = 0;
};

class NewDeleteAllocator final : public AllocatorI {
   protected:
    void* impl_try_allocate(USize size, USize alignment) noexcept override {
        if (alignment <= alignof(std::max_align_t)) [[likely]] {
            return ::operator new(size, std::nothrow);
        }

        return ::operator new(size, std::align_val_t(alignment), std::nothrow);
    }

    void impl_deallocate(void* pointer, USize size, USize alignment) noexcept override {
        if (alignment <= alignof(std::max_align_t)) [[likely]] {
            ::operator delete(pointer, size);
        } else {
            ::operator delete(pointer, size, std::align_val_t(alignment));
        }
    }
};

inline AllocatorI* get_new_delete_allocator() noexcept {
    static AllocatorI* pointer = [] {
        return static_cast<AllocatorI*>(new NewDeleteAllocator {});
    }();

    return pointer;
}

namespace impl {
inline std::atomic<AllocatorI*>& get_default_allocator_slot() noexcept {
    static std::atomic<AllocatorI*> slot { get_new_delete_allocator() };
    return slot;
}
}  // namespace impl

inline AllocatorI* get_default_allocator() noexcept {
    return impl::get_default_allocator_slot().load(std::memory_order_acquire);
}

inline AllocatorI* set_default_allocator(AllocatorI* allocator) noexcept {
    assert(allocator);
    if (!allocator) return get_default_allocator();

    return impl::get_default_allocator_slot().exchange(allocator,
                                                       std::memory_order_acq_rel);
}

}  // namespace duck
