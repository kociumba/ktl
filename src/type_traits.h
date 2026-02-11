#ifndef TYPE_TRAITS_H
#define TYPE_TRAITS_H

#include "common.h"

#include <concepts>
#include <type_traits>

namespace ktl_ns {

template <typename F, typename... Args>
concept nothrow_invocable = std::invocable<F, Args...> && std::is_nothrow_invocable_v<F, Args...>;

template <typename T>
concept numeric = std::integral<T> || std::floating_point<T>;

}  // namespace ktl_ns

#endif /* TYPE_TRAITS_H */
