#ifndef TYPE_TRAITS_H
#define TYPE_TRAITS_H

#include "common.h"

#include <concepts>
#include <type_traits>

#define ktl_noexcept_ctor_mv(type) noexcept(std::is_nothrow_move_constructible_v<type>)
#define ktl_noexcept_ctors_mv(...) noexcept(all_nothrow_move_v<__VA_ARGS__>)
#define ktl_noexcept_ctor_cpy(type) noexcept(std::is_nothrow_copy_constructible_v<type>)
#define ktl_noexcept_ctors_cpy(...) noexcept(all_nothrow_copy_v<__VA_ARGS__>)
#define ktl_noexcept_dtor(type) noexcept(std::is_nothrow_destructible_v<type>)
#define ktl_noexcept_mv(type) \
    noexcept(std::is_nothrow_move_constructible_v<type> && std::is_nothrow_destructible_v<type>)
#define ktl_noexcept_cpy(type) \
    noexcept(std::is_nothrow_copy_constructible_v<type> && std::is_nothrow_destructible_v<type>)

namespace ktl_ns {

template <typename... Ts>
inline constexpr bool all_nothrow_move_v = (std::is_nothrow_move_constructible_v<Ts> && ...);

template <typename... Ts>
inline constexpr bool all_nothrow_copy_v = (std::is_nothrow_copy_constructible_v<Ts> && ...);

template <typename F, typename... Args>
concept nothrow_invocable = std::invocable<F, Args...> && std::is_nothrow_invocable_v<F, Args...>;

template <typename T>
concept numeric = std::integral<T> || std::floating_point<T>;

}  // namespace ktl_ns

#endif /* TYPE_TRAITS_H */
