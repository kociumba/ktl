#ifndef KTL_ONCE_H
#define KTL_ONCE_H

#include "common.h"

#include <atomic>
#include <concepts>
#include <utility>

#define ktl_once(expr) \
    ktl_ns::detail::_once(ktl_ns::detail::_once_tag<__COUNTER__>{}, [&] { expr; })
#define ktl_once_safe(expr) \
    ktl_ns::detail::_once_safe(ktl_ns::detail::_once_tag<__COUNTER__>{}, [&] { expr; })

namespace ktl_ns::detail {

template <auto>
struct _once_tag {};

template <auto Id, std::invocable Func>
inline void _once(_once_tag<Id>, Func&& f) {
    [[no_unique_address]] static bool done = false;
    if (!done) {
        done = true;
        std::forward<Func>(f)();
    }
}

template <auto Id, std::invocable Func>
inline void _once_safe(_once_tag<Id>, Func&& f) {
    [[no_unique_address]] static std::atomic_flag done = ATOMIC_FLAG_INIT;
    if (!done.test_and_set(std::memory_order_relaxed)) { std::forward<Func>(f)(); }
}

}  // namespace ktl_ns::detail

#endif /* KTL_ONCE_H */
