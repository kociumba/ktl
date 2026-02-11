#ifndef KTL_GEOMETRY_PRIMITIVES_H
#define KTL_GEOMETRY_PRIMITIVES_H

#include "common.h"

#include "type_traits.h"

#include <cstddef>
#include <limits>

namespace ktl_ns {

template <numeric T>
struct pos2_t {
    static constexpr T invalid_value = std::numeric_limits<T>::max();
    static constexpr pos2_t<T> invalid() { return {invalid_value, invalid_value}; };

    T x, y;
};

using pos2_size = pos2_t<size_t>;

template <numeric T>
struct _rect {
    pos2_t<T> top_left, bottom_right;
};

using rect = _rect<size_t>;

template <numeric T>
struct pos3_t {
    static constexpr T invalid_value = std::numeric_limits<T>::max();
    static constexpr pos3_t<T> invalid() { return {invalid_value, invalid_value, invalid_value}; };

    T x, y, z;
};

using pos3_size = pos3_t<size_t>;

template <numeric T>
struct _box {
    pos3_t<T> min, max;
};

using box = _box<size_t>;

}  // namespace ktl_ns

#endif /* KTL_GEOMETRY_PRIMITIVES_H */
