// this header implements an arena allocator the implementation is based on:
//      https://github.com/tsoding/arena
//
// the arena is implemented as a linked list of regions, each region is a contiguous block of memory
// the design is adapted to support debug allocation tracing and the c++ allocator interface
//
// todo:
//  - add support for alignment in allocations

#ifndef ARENA_H
#define ARENA_H

#include "common.h"

#include <cstdarg>
#include <type_traits>

#define ktl_arena_assert(val) \
    ktl_assert(val);          \
    if (!val) { return nullptr; }

#if !defined(NDEBUG) && !defined(KTL_ARENA_NO_TRACING)
#define KTL_ARENA_TRACING
#endif

#define KTL_ARENA_BACKEND_LIBC 0
#define KTL_ARENA_BACKEND_VIRTUAL_MEMORY 1

#ifndef KTL_ARENA_BACKEND
// #define KTL_ARENA_BACKEND KTL_ARENA_BACKEND_LIBC
#define KTL_ARENA_BACKEND KTL_ARENA_BACKEND_VIRTUAL_MEMORY
#endif

#ifndef KTL_ARENA_REGION_DEFAULT_CAPACITY
#define KTL_ARENA_REGION_DEFAULT_CAPACITY (8 * 1024)
#endif

#if KTL_ARENA_BACKEND == KTL_ARENA_BACKEND_LIBC
#include <stdlib.h>
#elif KTL_ARENA_BACKEND == KTL_ARENA_BACKEND_VIRTUAL_MEMORY
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__unix__) || defined(__APPLE__) || defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#else
#error KTL_ARENA_BACKEND_VIRTUAL_MEMORY is only supported on windows and unix like platofrms
#endif
#endif

namespace ktl_ns {

struct Region;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200)
#endif

struct Region {
    Region* next = nullptr;
    size_t count = 0;
    size_t capacity = 0;
    uintptr_t data[];
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

struct Arena {
    Region *begin = nullptr, *end = nullptr;

#if defined(KTL_ARENA_TRACING)
    size_t region_creations = 0;
    size_t allocations_bigger_than_region_size = 0;
#endif
};

struct ArenaSnapshot {
    Region* region = nullptr;
    size_t count = 0;
};

namespace detail {

inline Region* new_region(size_t capacity);
inline void free_region(Region* r);

#if KTL_ARENA_BACKEND == KTL_ARENA_BACKEND_LIBC

inline Region* new_region(size_t capacity) {
    size_t size = sizeof(Region) + sizeof(uintptr_t) * capacity;
    auto r = (Region*)malloc(size);
    ktl_arena_assert(r);

    r->next = nullptr;
    r->count = 0;
    r->capacity = capacity;
    return r;
}

inline void free_region(Region* r) { free(r); }

#elif KTL_ARENA_BACKEND == KTL_ARENA_BACKEND_VIRTUAL_MEMORY

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN

inline Region* new_region(size_t capacity) {
    size_t size = sizeof(Region) + sizeof(uintptr_t) * capacity;
    auto r = (Region*)VirtualAllocEx(
        GetCurrentProcess(), nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (!r || r == INVALID_HANDLE_VALUE) { ktl_arena_assert(0 && "VirtualAllocEx() failed"); }

    r->next = nullptr;
    r->count = 0;
    r->capacity = capacity;
    return r;
}

inline void free_region(Region* r) {
    if (!r || r == INVALID_HANDLE_VALUE) { return; }

    bool free_res = VirtualFreeEx(GetCurrentProcess(), (LPVOID)r, 0, MEM_RELEASE);

    if (!free_res) { ktl_assert(0 && "VirtualFreeEx() failed."); }
}

#elif defined(__unix__) || defined(__APPLE__) || defined(__linux__)

inline Region* new_region(size_t capacity) {
    size_t size = sizeof(Region) + sizeof(uintptr_t) * capacity;
    auto r =
        (Region*)mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    ktl_arena_assert(r != MAP_FAILED);

    r->next = nullptr;
    r->count = 0;
    r->capacity = capacity;
    return r;
}

inline void free_region(Region* r) {
    size_t size = sizeof(Region) + sizeof(uintptr_t) * r->capacity;
    int ret = munmap(r, size);
    ktl_arena_assert(ret == 0);
}

#else
#error KTL_ARENA_BACKEND_VIRTUAL_MEMORY is only supported on windows and unix like platofrms
#endif

#endif

}  // namespace detail

inline void* arena_alloc(Arena* a, size_t bytes) {
    size_t size = (bytes + sizeof(uintptr_t) - 1) / sizeof(uintptr_t);

    if (a->end == nullptr) {
        ktl_arena_assert(!a->begin);
        size_t capacity = KTL_ARENA_REGION_DEFAULT_CAPACITY;
        if (capacity < size) {
            capacity = size;
#if defined(KTL_ARENA_TRACING)
            a->allocations_bigger_than_region_size++;
#endif
        }

        a->end = detail::new_region(capacity);
#if defined(KTL_ARENA_TRACING)
        a->region_creations++;
#endif
        a->begin = a->end;
    }

    while (a->end->count + size > a->end->capacity && a->end->next != nullptr) {
        a->end = a->end->next;
    }

    if (a->end->count + size > a->end->capacity) {
        ktl_arena_assert(!a->end->next);
        size_t capacity = KTL_ARENA_REGION_DEFAULT_CAPACITY;
        if (capacity < size) { capacity = size; }
        a->end->next = detail::new_region(capacity);
#if defined(KTL_ARENA_TRACING)
        a->region_creations++;
#endif
        a->end = a->end->next;
    }

    void* result = &a->end->data[a->end->count];
    a->end->count += size;
    return result;
}

inline void* arena_realloc(Arena* a, void* oldptr, size_t oldsz, size_t newsz) {
    if (newsz <= oldsz) { return oldptr; }
    void* newptr = arena_alloc(a, newsz);
    char* newptr_char = (char*)newptr;
    char* oldptr_char = (char*)oldptr;

    for (size_t i = 0; i < oldsz; ++i) {
        newptr_char[i] = oldptr_char[i];
    }

    return newptr;
}

inline ArenaSnapshot arena_snapshot(Arena* a) {
    ArenaSnapshot s;
    if (!a->end) {
        ktl_assert(!a->begin);
        s.region = a->end;
        s.count = 0;
    } else {
        s.region = a->end;
        s.count = a->end->count;
    }

    return s;
}

inline void arena_reset(Arena* a) {
    for (Region* r = a->begin; r; r = r->next) {
        r->count = 0;
    }

    a->end = a->begin;
}

inline void arena_rewind(Arena* a, ArenaSnapshot s) {
    if (!s.region) {
        arena_reset(a);
        return;
    }

    s.region->count = s.count;
    for (Region* r = s.region->next; r; r = r->next) {
        r->count = 0;
    }

    a->end = s.region;
}

inline void arena_free(Arena* a) {
    Region* r = a->begin;

    while (r) {
        Region* r0 = r;
        r = r->next;
        detail::free_region(r0);
    }

    a->begin = nullptr;
    a->end = nullptr;
}

inline void arena_trim(Arena* a) {
    Region* r = a->end->next;

    while (r) {
        Region* r0 = r;
        r = r->next;
        detail::free_region(r0);
    }

    a->end->next = nullptr;
}

#ifdef __cplusplus

template <typename T>
struct ArenaAllocator {
    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using propagate_on_container_move_assignment = std::true_type;
    using is_always_equal = std::false_type;

    Arena* arena;

    ArenaAllocator() noexcept : arena(nullptr) {}
    explicit ArenaAllocator(Arena* a) noexcept : arena(a) {}

    ArenaAllocator(const ArenaAllocator& other) noexcept = default;

    template <typename U>
    ArenaAllocator(const ArenaAllocator<U>& other) noexcept : arena(other.arena) {}

    [[nodiscard]] T* allocate(size_t n) {
#if !defined(NDEBUG)
        if (n > size_t(-1) / sizeof(T)) { return nullptr; }
#endif
        return static_cast<T*>(arena_alloc(arena, n * sizeof(T)));
    }

    void deallocate(T*, size_t) noexcept {}

    template <typename U>
    bool operator==(const ArenaAllocator<U>& other) const noexcept {
        return arena == other.arena;
    }

    template <typename U>
    bool operator!=(const ArenaAllocator<U>& other) const noexcept {
        return arena != other.arena;
    }
};

#endif

}  // namespace ktl_ns

#endif /* ARENA_H */
