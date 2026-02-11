#ifndef KTL_COMMON_H
#define KTL_COMMON_H

#if defined(KTL_NAMESPACE)
#define ktl_ns KTL_NAMESPACE
#else
#define ktl_ns ktl
#endif

#if defined(KTL_ASSERT)
#define ktl_assert KTL_ASSERT
#else
#include <cassert>
#define ktl_assert assert
#endif

#endif /* KTL_COMMON_H */
