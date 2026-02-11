#ifndef KTL_DEFER_H
#define KTL_DEFER_H

#include "common.h"

#include "type_traits.h"

#include <functional>
#include <utility>

namespace ktl_ns {

template <nothrow_invocable Func>
struct ScopeGuard {
    Func f;

    explicit ScopeGuard(Func&& func) noexcept : f(std::forward<Func>(func)) {}

    ~ScopeGuard() noexcept { std::invoke(f); }
};

#define defer(func) ktl_ns::ScopeGuard qk_defer_##__LINE__([&] { func; })
#define defer_val(func) ktl_ns::ScopeGuard qk_defer_##__LINE__([=] { func; })
#define defer_raw(func) ktl_ns::ScopeGuard qk_defer_##__LINE__(func)

}  // namespace ktl_ns

#endif /* KTL_DEFER_H */
