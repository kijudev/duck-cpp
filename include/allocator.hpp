#pragma once

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <new>

#include "impl.hpp"
#include "typedefs.hpp"

#if defined(_WIN32)
#include <malloc.h>
#endif

namespace duck {

enum class OOMHandlerAction : U8 { Fail, Retry };

using OOMHandler = OOMHandlerAction (*)(USize sz, USize alignment) noexcept;

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

enum class OwnershipResult : U8 {
    Owns,
    DoesNotOwn,
    Unknown,
};

class Allocator {
   public:
    virtual ~Allocator() = default;

    [[nodiscard("Discarding the pointer leads to memory leaks!")]] void* allocate(
        USize sz, USize alignment) noexcept {
        void* result = try_allocate(sz, alignment);

        // @todo Implement custom macros.
        if (!result) {
            std::abort();
        }

        return result;
    }

    [[nodiscard("Discarding the pointer leads to memory leaks!")]] void* reallocate(
        void* ptr, USize old_sz, USize new_sz, USize alignment) noexcept {
        void* result = try_reallocate(ptr, old_sz, new_sz, alignment);

        // @todo Implement custom macros.
        if (!result) {
            std::abort();
        }

        return result;
    }

    [[nodiscard("Discarding the pointer leads to memory leaks!")]] void* try_allocate(
        USize sz, USize alignment) noexcept {
        assert(sz != 0);
        assert(impl::is_valid_alignment(alignment));

        U8 max_retries = get_oom_retries();
        for (U8 attempt = 0;; ++attempt) {
            if (void* result = do_try_allocate(sz, alignment)) return result;
            if (attempt >= max_retries) return nullptr;

            OOMHandler oom_handler = get_oom_handler();
            if (oom_handler == nullptr) return nullptr;
            if (oom_handler(sz, alignment) != OOMHandlerAction::Retry) return nullptr;
        }
    }

    [[nodiscard("Discarding the pointer leads to memory leaks!")]] void* try_reallocate(
        void* ptr, USize old_sz, USize new_sz, USize alignment) noexcept {
        assert(ptr);
        assert(old_sz != 0);
        assert(new_sz != 0);
        assert(impl::is_valid_alignment(alignment));

        U8 max_retries = get_oom_retries();
        for (U8 attempt = 0;; ++attempt) {
            if (void* result = do_try_reallocate(ptr, old_sz, new_sz, alignment)) {
                return result;
            }
            if (attempt >= max_retries) return nullptr;

            OOMHandler oom_handler = get_oom_handler();
            if (oom_handler == nullptr) return nullptr;
            if (oom_handler(new_sz, alignment) != OOMHandlerAction::Retry) return nullptr;
        }
    }

    void deallocate(void* ptr, USize sz, USize alignment) noexcept {
        if (!ptr) return;

        assert(sz != 0);
        assert(impl::is_valid_alignment(alignment));

        do_deallocate(ptr, sz, alignment);
    }

    virtual OwnershipResult debug_owns(void* /*ptr*/) const noexcept {
        return OwnershipResult::Unknown;
    };

    virtual const char* debug_name() const noexcept { return "UnnamedAllocator"; };

   protected:
    virtual void* do_try_allocate(USize sz, USize alignment) noexcept = 0;

    virtual void* do_try_reallocate(void* ptr, USize old_sz, USize new_sz,
                                    USize alignment) noexcept {
        void* new_pointer = do_try_allocate(new_sz, alignment);
        if (!new_pointer) return nullptr;

        Byte* dst = static_cast<Byte*>(new_pointer);
        Byte* src = static_cast<Byte*>(ptr);
        USize n   = std::min(old_sz, new_sz);

        std::memcpy(dst, src, n);
        do_deallocate(ptr, old_sz, alignment);

        return dst;
    }

    virtual void do_deallocate(void* ptr, USize sz, USize alignment) noexcept = 0;
};

class NewDeleteAllocator final : public Allocator {
   protected:
    void* do_try_allocate(USize sz, USize alignment) noexcept override {
        if (impl::is_overaligned(alignment)) [[unlikely]] {
            return ::operator new(sz, std::align_val_t(alignment), std::nothrow);
        }

        return ::operator new(sz, std::nothrow);
    }

    void do_deallocate(void* ptr, USize sz, USize alignment) noexcept override {
        // All overloads of `::operator delete` are noexcept by default.

        if (impl::is_overaligned(alignment)) [[unlikely]] {
            ::operator delete(ptr, sz, std::align_val_t(alignment));
        } else {
            ::operator delete(ptr, sz);
        }
    }

    OwnershipResult debug_owns(void* /*ptr*/) const noexcept override {
        return OwnershipResult::Unknown;
    }

    const char* debug_name() const noexcept override { return "NewDeleteAllocator"; }
};

class MallocAllocator final : public Allocator {
   protected:
    void* do_try_allocate(USize sz, USize alignment) noexcept override {
        if (!impl::is_overaligned(alignment)) [[likely]] {
            return std::malloc(sz);
        }

#if defined(_WIN32)
        return _aligned_malloc(sz, alignment);
#else
        void* ptr = nullptr;
        if (S32 result = ::posix_memalign(&ptr, static_cast<USize>(alignment),
                                          static_cast<USize>(sz));
            result != 0) {
            return nullptr;
        }

        return ptr;
#endif
    }

    void* do_try_reallocate(void* ptr, USize old_sz, USize new_sz,
                            USize alignment) noexcept override {
        if (!impl::is_overaligned(alignment)) [[likely]] {
            return std::realloc(ptr, new_sz);
        }

#if defined(_WIN32)
        return _aligned_realloc(ptr, new_sz, alignment);
#else
        return Allocator::do_try_reallocate(ptr, old_sz, new_sz, alignment);
#endif
    }

    void do_deallocate(void* ptr, USize /*size*/, USize alignment) noexcept override {
        if (!impl::is_overaligned(alignment)) [[likely]] {
            std::free(ptr);
            return;
        }

#if defined(_WIN32)
        _aligned_free(ptr);
#else
        std::free(ptr);  // posix_memalign memory is freed with regular free
#endif
    }

    OwnershipResult debug_owns(void* /*ptr*/) const noexcept override {
        return OwnershipResult::Unknown;
    }

    const char* debug_name() const noexcept override { return "MallocAllocator"; }
};

inline Allocator* get_new_delete_allocator() noexcept {
    static NewDeleteAllocator a;
    return &a;
}

inline Allocator* get_malloc_allocator() noexcept {
    static MallocAllocator a;
    return &a;
}

namespace impl {
inline std::atomic<Allocator*>& get_default_allocator_slot() noexcept {
    static std::atomic<Allocator*> slot { get_new_delete_allocator() };
    return slot;
}
}  // namespace impl

inline Allocator* get_default_allocator() noexcept {
    return impl::get_default_allocator_slot().load(std::memory_order_acquire);
}

inline Allocator* set_default_allocator(Allocator* allocator) noexcept {
    assert(allocator);
    if (!allocator) return get_default_allocator();

    return impl::get_default_allocator_slot().exchange(allocator,
                                                       std::memory_order_acq_rel);
}
}  // namespace duck
