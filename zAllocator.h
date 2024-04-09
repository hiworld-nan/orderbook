#pragma once

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>
#include "flatPool.h"

template <class T>
struct zAllocator : public std::allocator<T> {
   public:
    using value_type = T;

    using pointer = T*;
    using const_pointer = const T*;

    using reference = T&;
    using const_reference = const T&;

    using size_type = size_t;
    using difference_type = std::ptrdiff_t;

    using propagate_on_container_move_assignment = std::true_type;

    template <class U>
    struct rebind {
        using other = zAllocator<U>;
    };

    zAllocator() = default;
    ~zAllocator() = default;

    zAllocator(zAllocator&&) = delete;
    zAllocator(const zAllocator&) = delete;
    zAllocator& operator=(zAllocator&& other) = delete;
    zAllocator& operator=(const zAllocator& other) = delete;

    // todo: custom allocator for unordered container
    template <class U>
    zAllocator(const zAllocator<U>&) noexcept {}

    void deallocate(pointer p, size_type n = 1) { memPool_.dealloc(p); }
    pointer allocate(size_type n, const void* hint = 0) { return memPool_.alloc(); }

    void destroy(pointer p) { memPool_.dealloc(p); }

    template <class U = T, class... Params>
    void construct(U* p, Params&&... params) {
        ::new (p) U(std::forward<Params>(params)...);
    }
    void construct(pointer p, const T& value) { ::new (p) T(value); }

    pointer address(reference ref) { return &ref; }
    const_pointer address(const_reference cref) { return &cref; }

    size_type max_size() const { return memPool_.max_size(); }
    FlatPool<T>& get_pool() { return memPool_; }

   private:
    FlatPool<T> memPool_;
};
