#ifndef RESULT_H
#define RESULT_H

#include "common.h"

#include <tuple>
#include "type_traits.h"

namespace ktl_ns {

template <typename ERR>
struct error_t {
    ERR err;
};

template <typename ERR>
struct _error {
    bool has_error;
    union {
        char nil;
        ERR err;
    };

    _error() noexcept : has_error(false), nil(0) {}

    explicit _error(ERR&& e) ktl_noexcept_ctor_mv(ERR) : has_error(true), err(std::move(e)) {}
    explicit _error(const ERR& e) ktl_noexcept_ctor_cpy(ERR) : has_error(true), err(e) {}

    _error(const _error& other) ktl_noexcept_ctor_cpy(ERR) : has_error(other.has_error) {
        if (has_error) {
            new (&err) ERR(other.err);
        } else {
            nil = 0;
        }
    }

    _error(_error&& other) ktl_noexcept_ctor_mv(ERR) : has_error(other.has_error) {
        if (has_error) {
            new (&err) ERR(std::move(other.err));
        } else {
            nil = 0;
        }
    }

    _error& operator=(const _error& other) ktl_noexcept_cpy(ERR) {
        if (this == &other) return *this;
        if (has_error) err.~ERR();
        has_error = other.has_error;
        if (has_error) {
            new (&err) ERR(other.err);
        } else {
            nil = 0;
        }
        return *this;
    }

    _error& operator=(_error&& other) ktl_noexcept_mv(ERR) {
        if (this == &other) return *this;
        if (has_error) err.~ERR();
        has_error = other.has_error;
        if (has_error) {
            new (&err) ERR(std::move(other.err));
        } else {
            nil = 0;
        }
        return *this;
    }

    operator bool() const noexcept { return has_error; }

    operator ERR() const& ktl_noexcept_ctor_cpy(ERR) {
        ktl_assert(has_error);
        return err;
    }

    operator ERR() && ktl_noexcept_ctor_mv(ERR) {
        ktl_assert(has_error);
        return std::move(err);
    }

    ERR& operator*() & {
        ktl_assert(has_error);
        return err;
    }

    const ERR& operator*() const& {
        ktl_assert(has_error);
        return err;
    }

    ERR&& operator*() && {
        ktl_assert(has_error);
        return std::move(err);
    }

    ~_error() ktl_noexcept_dtor(ERR) {
        if (has_error) { err.~ERR(); }
    }
};

template <typename VAL, typename ERR>
    requires std::is_default_constructible_v<VAL> && (!std::is_convertible_v<VAL, error_t<ERR>>)
struct [[nodiscard]] result {
    VAL val;
    _error<ERR> err;

    result(const VAL& v) ktl_noexcept_ctor_cpy(VAL) : val(v), err() {}
    result(VAL&& v) ktl_noexcept_ctor_mv(VAL) : val(std::move(v)), err() {}

    result(error_t<ERR>&& e) ktl_noexcept_ctor_mv(ERR) : val(), err(std::move(e.err)) {}
    result(const error_t<ERR>& e) ktl_noexcept_ctor_cpy(ERR) : val(), err(e.err) {}

    result(const VAL& v, const ERR& e) ktl_noexcept_ctors_cpy(VAL, ERR) : val(v), err(e) {}

    result(VAL&& v, ERR&& e) ktl_noexcept_ctors_mv(VAL, ERR)
        : val(std::move(v)), err(std::move(e)) {}

    explicit operator bool() const noexcept { return !err; }
};

template <typename ERR>
error_t<std::decay_t<ERR>> err(ERR&& err) ktl_noexcept_ctor_mv(ERR) {
    return {std::forward<ERR>(err)};
}

template <std::size_t I, typename VAL, typename ERR>
constexpr decltype(auto) get(ktl_ns::result<VAL, ERR>& r) noexcept {
    if constexpr (I == 0)
        return (r.val);
    else
        return (r.err);
}

template <std::size_t I, typename VAL, typename ERR>
constexpr decltype(auto) get(const ktl_ns::result<VAL, ERR>& r) noexcept {
    if constexpr (I == 0)
        return (r.val);
    else
        return (r.err);
}

template <std::size_t I, typename VAL, typename ERR>
constexpr decltype(auto) get(ktl_ns::result<VAL, ERR>&& r) noexcept {
    if constexpr (I == 0)
        return std::move(r.val);
    else
        return std::move(r.err);
}

template <std::size_t I, typename VAL, typename ERR>
constexpr decltype(auto) get(const ktl_ns::result<VAL, ERR>&& r) noexcept {
    if constexpr (I == 0)
        return std::move(r.val);
    else
        return std::move(r.err);
}

}  // namespace ktl_ns

namespace std {

template <typename VAL, typename ERR>
struct tuple_size<ktl_ns::result<VAL, ERR>> : std::integral_constant<std::size_t, 2> {};

template <std::size_t I, typename VAL, typename ERR>
struct tuple_element<I, ktl_ns::result<VAL, ERR>>;

template <typename VAL, typename ERR>
struct tuple_element<0, ktl_ns::result<VAL, ERR>> {
    using type = VAL;
};

template <typename VAL, typename ERR>
struct tuple_element<1, ktl_ns::result<VAL, ERR>> {
    using type = ktl_ns::_error<ERR>;
};

}  // namespace std

#endif /* RESULT_H */
