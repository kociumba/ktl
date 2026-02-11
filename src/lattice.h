#ifndef LATTICE_H
#define LATTICE_H

#include "common.h"

#include "geometry_primitives.h"
#include "intrinsics.h"

#include <algorithm>
#include <concepts>
#include <memory>
#include <utility>
#include <vector>
#include <generator>

#if !defined(NDEBUG) && !defined(KTL_LATTICE_NO_SAFE_ACCESS)
#define KTL_LATTICE_SAFE_ACCESS
#endif

namespace ktl_ns {

enum lattice_opts : size_t {
    LATTICE_FIXED_CENTER = 1 << 0,  // Grows equally in all directions from the center (default)
    LATTICE_FIXED_X_MIN = 1 << 1,   // Grows toward +X (Right)
    LATTICE_FIXED_X_MAX = 1 << 2,   // Grows toward -X (Left)
    LATTICE_FIXED_Y_MIN = 1 << 3,   // Grows toward +Y (Up)
    LATTICE_FIXED_Y_MAX = 1 << 4,   // Grows toward -Y (Down)
    LATTICE_FIXED_Z_MIN = 1 << 5,   // Grows toward +Z (Forward)
    LATTICE_FIXED_Z_MAX = 1 << 6,   // Grows toward -Z (Back)

    LATTICE_FIXED_ORIGIN = LATTICE_FIXED_X_MIN | LATTICE_FIXED_Y_MIN | LATTICE_FIXED_Z_MIN,

    LATTICE_FIXED_SIZE_X = LATTICE_FIXED_X_MIN | LATTICE_FIXED_X_MAX,
    LATTICE_FIXED_SIZE_Y = LATTICE_FIXED_Y_MIN | LATTICE_FIXED_Y_MAX,
    LATTICE_FIXED_SIZE_Z = LATTICE_FIXED_Z_MIN | LATTICE_FIXED_Z_MAX,

    LATTICE_NO_GROW = LATTICE_FIXED_SIZE_X | LATTICE_FIXED_SIZE_Y | LATTICE_FIXED_SIZE_Z,

    LATTICE_NO_RETAIN_STATE = 1 << 7,
};

template <typename T, typename Alloc = std::allocator<T>>
struct lattice {
    std::vector<T, Alloc> data;
    size_t width, height, depth;
    size_t opts;

    lattice(size_t width,
        size_t height,
        size_t depth,
        T def_val = T{},
        size_t opts = LATTICE_FIXED_CENTER)
        : width(width), height(height), depth(depth), opts(opts) {
        size_t xy;
        auto of1 = mul_overflow_size(width, height, &xy);
        ktl_assert(!of1);
        auto of2 = mul_overflow_size(xy, depth, nullptr);
        ktl_assert(!of2);
        data.assign(width * height * depth, def_val);
    }

    lattice(pos3_size dim, T def_val = T{}, size_t opts = LATTICE_FIXED_CENTER)
        : lattice(dim.x, dim.y, dim.z, def_val, opts) {}

    template <typename T>
    struct slice {
        T* _slice;
        size_t height;
        size_t width;

        operator T*() const { return _slice; }
        slice(T* p, size_t h = 0, size_t w = 0) : _slice(p), height(h), width(w) {}

        row<T>& operator[](size_t z) {
#if defined(KTL_LATTICE_SAFE_ACCESS)
            ktl_assert(h < height);
#endif
            return row<T>{_slice[h * width], width};
        }
    };

    template <typename T>
    struct row {
        T* _row;
        size_t width;

        operator T*() const { return _row; }
        row(T* p, size_t w = 0) : _row(p), width(w) {}

        T& operator[](size_t x) {
#if defined(KTL_LATTICE_SAFE_ACCESS)
            ktl_assert(w < width);
#endif
            return _row[x];
        }
    };

    slice<T> operator[](size_t z) {
#if defined(KTL_LATTICE_SAFE_ACCESS)
        ktl_assert(z < depth);
#endif
        return slice<T>{&data[z * width * height], width, height};
    }

    // direct access to coordinate (x, y, z) without proxy types
    inline T& xyz(size_t x, size_t y, size_t z) { return data[z * width * height][y * width][x]; }
    inline const T& xyz(size_t x, size_t y, size_t z) const { return (const T&)xyz(x, y, z); }
    // direct access to coordinate at pos without proxy types
    inline T& xyz(pos3_size pos) { return xyz(pos.x, pos.y, pos.z); }
    // accesses the cell at (x, y, z) with bound checking based on the configured assert
    inline T& at(size_t x, size_t y, size_t z) {
        ktl_assert(in_bounds(x, y, z));
        return xyz(x, y, z);
    }
    inline T& at(pos3_size pos) { return at(pos.x, pos.y, pos.z); }

    inline bool in_bounds(pos3_size p) const { return in_bounds(p.x, p.y, p.z); }
    // checks if the coordinates (x, y, z) are within the bounds of the lattice
    bool in_bounds(size_t x, size_t y, size_t z) const {
        return x < width && y < height && z < depth;
    }

    size_t to_idx(size_t x, size_t y, size_t z) const { to_idx({x, y, z}); }
    size_t to_idx(pos3_size p) const {
        if (!in_bounds(p)) {
            ktl_assert(false);
            return 0;
        }
        return (p.z * height * width) + (p.y * width) + p.x;
    }

    pos3_size to_pos(size_t idx) const {
        if (idx >= width * height * depth) {
            ktl_assert(false);
            return pos3_size::invalid();
        }
        size_t z = idx / (width * height);
        size_t y = (idx % (width * height)) / width;
        size_t x = idx % width;
        return {x, y, z};
    }

    // fills the entire lattice with the specified value
    void fill(T& val) { data.assign(width * height * depth, val); }
    // fills the specified bounding box with the specified value
    void fill(const box& r, const T& val) {
        ktl_assert(in_bounds(r.min));
        ktl_assert(in_bounds(r.max));
        for (size_t z = r.min.z; z <= r.max.z; z++) {
            for (size_t y = r.min.y; y <= r.max.y; y++) {
                for (size_t x = r.min.x; x <= r.max.x; x++) {
                    xyz(x, y, z) = val;
                }
            }
        }
    }

    template <typename Func>
    requires std::predicate<Func, T&, pos3_size> void place_if(T& val, Func func) {
        for (size_t z = 0; z < depth; z++) {
            for (size_t y = 0; y < height; y++) {
                for (size_t x = 0; x < width; x++) {
                    if (func(xyz(x, y, z), {x, y, z})) { xyz(x, y, z) = val; }
                }
            }
        }
    }

    // iterates over the orthogonal neighbors of position p, calling func for each neighbor
    template <typename Func>
    requires std::predicate<Func, T&, pos3_size> inline void face_neighbors(pos3_size p, Func func) {
        face_neighbors(p.x, p.y, p.z, std::forward<Func>(func));
    }

    template <typename Func>
    requires std::predicate<Func, T&, pos3_size> inline void n6(pos3_size p, Func func) {
        face_neighbors(p, std::forward<Func>(func));
    }

    // iterates over the orthogonal neighbors of coordinates (x, y), calling func for each neighbor
    template <typename Func>
    requires std::predicate<Func, T&, pos3_size>
    void face_neighbors(size_t x, size_t y, size_t z, Func func) {
        if (x > 0) {
            if (!func(xyz(x - 1, y, z), pos3_size{x - 1, y, z})) { return; }
        }
        if (x + 1 < width) {
            if (!func(xyz(x + 1, y, z), pos3_size{x + 1, y, z})) { return; }
        }
        if (y > 0) {
            if (!func(xyz(x, y - 1, z), pos3_size{x, y - 1, z})) { return; }
        }
        if (y + 1 < height) {
            if (!func(xyz(x, y + 1, z), pos3_size{x, y + 1, z})) { return; }
        }
        if (z > 0) {
            if (!func(xyz(x, y, z - 1), pos3_size{x, y, z - 1})) { return; }
        }
        if (z + 1 < depth) {
            if (!func(xyz(x, y, z + 1), pos3_size{x, y, z + 1})) { return; }
        }
    }

    // iterates over the moore neighbors of position p, calling func for each neighbor
    template <typename Func>
    requires std::predicate<Func, T&, pos3_size> inline void full_neighbors(pos3_size p, Func func) {
        full_neighbors(p.x, p.y, p.z, std::forward<Func>(func));
    }

    template <typename Func>
    requires std::predicate<Func, T&, pos3_size> inline void n26(pos3_size p, Func func) {
        full_neighbors(p, std::forward<Func>(func));
    }

    // iterates over the moore neighbors of coordinates (x, y), calling func for each neighbor
    template <typename Func>
    requires std::predicate<Func, T&, pos3_size>
    void full_neighbors(size_t x, size_t y, size_t z, Func func) {
        for (int dz = (z > 0 ? -1 : 0); dz <= (z + 1 < depth ? 1 : 0); dz++) {
            for (int dy = (y > 0 ? -1 : 0); dy <= (y + 1 < height ? 1 : 0); dy++) {
                for (int dx = (x > 0 ? -1 : 0); dx <= (x + 1 < width ? 1 : 0); dx++) {
                    if (dx == 0 && dy == 0 && dz == 0) { continue; }
                    if (!func(xyz(x + dx, y + dy, z + dz), pos3_size{x + dx, y + dy, z + dz})) {
                        return;
                    }
                }
            }
        }
    }

    // finds the first occurrence of val in the lattice and returns its position, or an invalid position if not found
    pos3_size contains(T& val) {
        for (size_t z = 0; z < depth; z++) {
            for (size_t y = 0; y < height; y++) {
                for (size_t x = 0; x < width; x++) {
                    if ((xyz(x, y, z) == val) {
                        return pos3{x, y, z}; }
                }
            }
        }
        return pos3_size::invalid();
    }

    // finds all occurrences of val in the lattice and calls func for each occurrence, passing the value and its position
    template <typename Func>
    requires std::predicate<Func, T&, pos3_size> void find_all(T& val, Func func) {
        for (size_t z = 0; z < depth; z++) {
            for (size_t y = 0; y < height; y++) {
                for (size_t x = 0; x < width; x++) {
                    if (xyz(x, y, z) == val) {
                        if (!func(xyz(x, y, z), pos3_size{x, y, z})) { return; }
                    }
                }
            }
        }
    }

    // traverses the lattice starting from position start, calling predicate for each cell
    // if predicate returns true, calls transform for that cell
    template <typename Func>
    requires std::predicate<Func, T&, pos3_size>
    void traverse(pos3_size start, Func predicate, Func transform) {
        ktl_assert(in_bounds(start) && predicate(xyz(start.x, start.y, start.z), start));
        for (size_t z = start.z; z < depth; z++) {
            for (size_t y = start.y; y < height; y++) {
                for (size_t x = start.x; x < width; x++) {
                    if (!predicate(xyz(x, y, z), pos3_size{x, y, z})) { return; }
                    transform(xyz(x, y, z), pos3_size{x, y, z});
                }
            }
        }
    }

    void reset(T def_val = T{}) { data.assign(width * height * depth, def_val); }

    void resize(size_t new_width, size_t new_height, size_t new_depth, T def_val = T{}) {
        if (new_width == width && new_height == height && new_depth == depth) { return; }

        if (opts & LATTICE_NO_RETAIN_STATE) {
            data.assign(new_width * new_height * new_depth, def_val);
            width = new_width;
            height = new_height;
            depth = new_depth;
            return;
        }

        std::vector<T, Alloc> new_data(new_width * new_height * new_depth, def_val);

        size_t ox = 0, oy = 0, oz = 0;

        if (opts & LATTICE_FIXED_CENTER) {
            ox = (new_width > width) ? (new_width - width) / 2 : 0;
            oy = (new_height > height) ? (new_height - height) / 2 : 0;
            oz = (new_depth > depth) ? (new_depth - depth) / 2 : 0;
        } else {
            if (opts & LATTICE_FIXED_SIZE_X) { new_width = width; }
            if (opts & LATTICE_FIXED_SIZE_Y) { new_height = height; }
            if (opts & LATTICE_FIXED_SIZE_Z) { new_depth = depth; }

            if (new_width > width) {
                if (opts & LATTICE_FIXED_X_MIN) {
                    ox = 0;
                } else if (opts & LATTICE_FIXED_X_MAX) {
                    ox = new_width - width;
                }
            }

            if (new_height > height) {
                if (opts & LATTICE_FIXED_Y_MIN) {
                    oy = 0;
                } else if (opts & LATTICE_FIXED_Y_MAX) {
                    oy = new_height - height;
                }
            }

            if (new_depth > depth) {
                if (opts & LATTICE_FIXED_Z_MIN) {
                    oz = 0;
                } else if (opts & LATTICE_FIXED_Z_MAX) {
                    oz = new_depth - depth;
                }
            }
        }

        size_t cw = std::min(width, new_width);
        size_t ch = std::min(height, new_height);
        size_t cd = std::min(depth, new_depth);

        for (size_t z = 0; z < cd; ++z) {
            for (size_t y = 0; y < ch; ++y) {
                size_t old_idx = (z * width * height) + (y * width);
                size_t new_idx = ((z + oz) * new_width * new_height) + ((y + oy) * new_width) + ox;

                std::copy_n(&data[old_idx], cw, &new_data[new_idx]);
            }
        }

        data = std::move(new_data);
        width = new_width;
        height = new_height;
        depth = new_depth;
    }

    struct cell {
        T& value;
        pos3_size position;
    };

    auto items() -> std::generator<cell> {
        for (size_t z = 0; z < depth; z++) {
            for (size_t y = 0; y < height; y++) {
                for (size_t x = 0; x < width; x++) {
                    co_yield cell{xyz(x, y, z), {x, y, z}};
                }
            }
        }
    }

    struct const_cell {
        const T& value;
        pos3_size position;
    };

    auto items() const -> std::generator<const_cell> {
        for (size_t z = 0; z < depth; z++) {
            for (size_t y = 0; y < height; y++) {
                for (size_t x = 0; x < width; x++) {
                    co_yield const_cell{xyz(x, y, z), {x, y, z}};
                }
            }
        }
    }
};

}  // namespace ktl_ns

#endif /* LATTICE_H */
