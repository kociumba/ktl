#ifndef KTL_INTRINSICS_H
#define KTL_INTRINSICS_H

#include "common.h"

#include <cstdint>

#if defined(__GNUC__) || defined(__clang__)
#define KTL_HAS_BUILTIN_OVERFLOW 1
#elif defined(_MSC_VER)
#define KTL_HAS_BUILTIN_OVERFLOW 0
#include <intrin.h>
#else
#define KTL_HAS_BUILTIN_OVERFLOW 0
#endif

#define ktl_mul_overflow(a, b, result)       \
    _Generic((result),                       \
        uint64_t*: ktl_ns::mul_overflow_u64, \
        int64_t*: ktl_ns::mul_overflow_i64,  \
        size_t*: ktl_ns::mul_overflow_size)(a, b, result)

namespace ktl_ns {

inline bool mul_overflow_u64(uint64_t a, uint64_t b, uint64_t* result) {
#if KTL_HAS_BUILTIN_OVERFLOW
    return __builtin_mul_overflow(a, b, result);
#else
    uint64_t res = a * b;
    if (result) *result = res;
    if (a == 0 || b == 0) { return false; }
    return (res / a) != b;
#endif
}

inline bool mul_overflow_i64(int64_t a, int64_t b, int64_t* result) {
#if KTL_HAS_BUILTIN_OVERFLOW
    return __builtin_mul_overflow(a, b, result);
#else
    if (a == -1 && b == INT64_MIN) return true;
    if (b == -1 && a == INT64_MIN) return true;

    int64_t res = a * b;
    if (result) *result = res;
    if (a == 0 || b == 0) { return false; }
    return (res / a) != b;
#endif
}

inline bool mul_overflow_size(size_t a, size_t b, size_t* result) {
#if KTL_HAS_BUILTIN_OVERFLOW
    return __builtin_mul_overflow(a, b, result);
#else
    size_t res = a * b;
    if (result) *result = res;
    if (a == 0 || b == 0) { return false; }
    return (res / a) != b;
#endif
}

}  // namespace ktl_ns

#endif /* KTL_INTRINSICS_H */
