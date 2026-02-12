#ifndef GRID_H
#define GRID_H

#include "common.h"

#include "geometry_primitives.h"
#include "intrinsics.h"

#include <algorithm>
#include <bit>
#include <concepts>
#include <generator>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#if !defined(NDEBUG) && !defined(KTL_GRID_NO_SAFE_ACCESS)
#define KTL_GRID_SAFE_ACCESS
#endif

namespace ktl_ns {

// grid_opts determines the behavior of the grid
enum grid_opts : size_t {
    // grows the grid to the bottom and right (default)
    GRID_GROW_BOTTOM_RIGHT = 1 << 0,
    // grows the grid to the bottom and left
    GRID_GROW_BOTTOM_LEFT = 1 << 1,
    // grows the grid to the top and right
    GRID_GROW_TOP_RIGHT = 1 << 2,
    // grows the grid to the top and left
    GRID_GROW_TOP_LEFT = 1 << 3,
    // grows the grid outward assuming the center of the grid is the anchor point
    GRID_GROW_OUTWARD = 1 << 4,

    GRID_GROW_MASK = (GRID_GROW_BOTTOM_RIGHT | GRID_GROW_BOTTOM_LEFT | GRID_GROW_TOP_RIGHT |
                      GRID_GROW_TOP_LEFT | GRID_GROW_OUTWARD),

    // couses the grid to perform a "fast" resize, discards all existing data when resizing
    GRID_NO_RETAIN_STATE = 1 << 5,
};

// grid implements a 2D grid of elements T, and facilitates common operations
// e.g. accessing orthogonal or moore neighbors, flood fill, etc.
template <typename T, typename Alloc = std::allocator<T>>
struct grid {
    // the raw packed grid data
    std::vector<T, Alloc> data;
    size_t width, height;
    size_t opts;

    // constructs a grid of dimensions width * height, with the specified options and default value for cells
    grid(size_t width, size_t height, T def_val = T{}, size_t opts = GRID_GROW_BOTTOM_RIGHT)
        : width(width), height(height), opts(opts) {
        bool valid = _check_opts();
        ktl_assert(valid);
        ktl_assert(!mul_overflow_size(width, height, nullptr));
        if (valid) { data.assign(width * height, def_val); }
    }

#if defined(KTL_GRID_SAFE_ACCESS)
    // by default used in debug builds to provide bound checking and safe access to grid cells
    template <typename T>
    struct row {
        T* _row;
        size_t width;

        operator T*() const { return _row; }
        row(T* p, size_t w = 0) : _row(p), width(w) {}

        T& operator[](size_t x) {
            ktl_assert(x < width);
            return _row[x];
        }
    };

    row<T> operator[](size_t y) {
        ktl_assert(y < height);
        return row{&data[y * width], width};
    }

    row<const T> operator[](size_t y) const {
        ktl_assert(y < height);
        return row{&data[y * width], width};
    }
#else
    // by default used in release builds, provides direct access to the grid data
    template <typename T>
    using row = T*;

    T* operator[](size_t y) { return &data[y * width]; }
    const T* operator[](size_t y) const { return &data[y * width]; }
#endif

    // accesses the cell at (x, y)
    T& xy(size_t x, size_t y) { return (*this)[y][x]; }
    // accesses the cell at (x, y) with bound checking based on the configured assert
    T& at(size_t x, size_t y) {
        ktl_assert(x < width && y < height);
        return (*this)[y][x];
    }
    // accesses the cell at position p with bound checking based on the configured assert
    T& at(pos2_size p) { return at(p.x, p.y); }
    // checks if the position p is within the bounds of the grid
    bool in_bounds(pos2_size p) const { return in_bounds(p.x, p.y); }
    // checks if the coordinates (x, y) are within the bounds of the grid
    bool in_bounds(size_t x, size_t y) const { return x < width && y < height; }

    // fills the entire grid with the specified value
    void fill(T& val) { data.assign(width * height, val); }
    // fills the specified rectangular area with the specified value
    void fill(rect r, T& val) {
        ktl_assert(in_bounds(r.top_left));
        ktl_assert(in_bounds(r.bottom_right));
        for (size_t y = r.top_left.y; y <= r.bottom_right.y; y++) {
            for (size_t x = r.top_left.x; x <= r.bottom_right.x; x++) {
                xy(x, y) = val;
            }
        }
    }

    // places the specified value in all cells where the provided function returns true
    template <typename Func>
    requires std::predicate<Func, T&, pos2_size> void place_if(T& val, Func func) {
        for (size_t y = 0; y < height; y++) {
            for (size_t x = 0; x < width; x++) {
                if (func((*this)[y][x], {x, y})) { (*this)[y][x] = val; }
            }
        }
    }

    // iterates over the orthogonal neighbors of position p, calling func for each neighbor
    template <typename Func>
    requires std::predicate<Func, T&, pos2_size> void orthogonal_neighbors(pos2_size p, Func func) {
        orthogonal_neighbors(p.x, p.y, std::forward<Func>(func));
    }

    // iterates over the orthogonal neighbors of coordinates (x, y), calling func for each neighbor
    template <typename Func>
    requires std::predicate<Func, T&, pos2_size>
    void orthogonal_neighbors(size_t x, size_t y, Func func) {
        if (x > 0) {
            if (!func((*this)[y][x - 1], pos2_size{x - 1, y})) { return; }
        }
        if (x + 1 < width) {
            if (!func((*this)[y][x + 1], pos2_size{x + 1, y})) { return; }
        }
        if (y > 0) {
            if (!func((*this)[y - 1][x], pos2_size{x, y - 1})) { return; }
        }
        if (y + 1 < height) {
            if (!func((*this)[y + 1][x], pos2_size{x, y + 1})) { return; }
        }
    }

    // iterates over the moore neighbors of position p, calling func for each neighbor
    template <typename Func>
    requires std::predicate<Func, T&, pos2_size> void moore_neighbors(pos2_size p, Func func) {
        moore_neighbors(p.x, p.y, std::forward<Func>(func));
    }

    // iterates over the moore neighbors of coordinates (x, y), calling func for each neighbor
    template <typename Func>
    requires std::predicate<Func, T&, pos2_size>
    void moore_neighbors(size_t x, size_t y, Func func) {
        for (int dy = (y > 0 ? -1 : 0); dy <= (y + 1 < height ? 1 : 0); dy++) {
            for (int dx = (x > 0 ? -1 : 0); dx <= (x + 1 < width ? 1 : 0); dx++) {
                if (dx == 0 && dy == 0) { continue; }
                if (!func((*this)[y + dy][x + dx], pos2_size{x + dx, y + dy})) { return; }
            }
        }
    }

    // finds the first occurrence of val in the grid and returns its position, or an invalid position if not found
    pos2_size contains(T& val) {
        for (size_t y = 0; y < height; y++) {
            for (size_t x = 0; x < width; x++) {
                if ((*this)[y][x] == val) { return {x, y}; }
            }
        }
        return pos2_size::invalid();
    }

    // finds all occurrences of val in the grid and calls func for each occurrence, passing the value and its position
    template <typename Func>
    requires std::predicate<Func, T&, pos2_size> void find_all(T& val, Func func) {
        for (size_t y = 0; y < height; y++) {
            for (size_t x = 0; x < width; x++) {
                if ((*this)[y][x] == val) {
                    if (!func((*this)[y][x], pos2_size{x, y})) { return; }
                }
            }
        }
    }

    // traverses the grid starting from position start, calling predicate for each cell
    // if predicate returns true, calls transform for that cell
    template <typename FuncP, typename FuncT>
    requires std::predicate<FuncP, T&, pos2_size> && std::invocable<FuncT, T&, pos2_size>
    void traverse(pos2_size start, FuncP predicate, FuncT transform) {
        ktl_assert(in_bounds(start) && predicate((*this)[start.y][start.x], start));
        for (size_t y = start.y; y < height; y++) {
            size_t current_x = (y == start.y) ? start.x : 0;
            for (size_t x = current_x; x < width; x++) {
                if (!predicate((*this)[y][x], pos2_size{x, y})) { return; }
                transform((*this)[y][x], pos2_size{x, y});
            }
        }
    }

    // core implementation of flood fill and related operations
    //
    // returns the positions of all cells that match the predicate and are connected to the start position
    template <typename Func>
    requires std::predicate<Func, T&, pos2_size>
    auto _flood_fill_core(pos2_size start, Func predicate, bool orthogonal) {
        using PosAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<pos2_size>;
        using BoolAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<bool>;

        std::vector<pos2_size, PosAlloc> visited_positions;

        if (!in_bounds(start) || !predicate((*this)[start.y][start.x], start)) {
            return visited_positions;
        }

        std::vector<bool, BoolAlloc> visited(width * height, false);
        auto flat_index = [this](pos2_size p) -> size_t { return p.y * width + p.x; };

        std::vector<pos2_size, PosAlloc> stack;
        size_t start_idx = flat_index(start);
        visited[start_idx] = true;
        visited_positions.push_back(start);
        stack.push_back(start);

        auto n_method = [&](pos2_size p, auto&& f) {
            if (orthogonal)
                this->orthogonal_neighbors(p, f);
            else
                this->moore_neighbors(p, f);
        };

        while (!stack.empty()) {
            pos2_size current = stack.back();
            stack.pop_back();

            n_method(current, [&](T& cell, pos2_size p) {
                size_t idx = flat_index(p);
                if (!visited[idx] && predicate(cell, p)) {
                    visited[idx] = true;
                    visited_positions.push_back(p);
                    stack.push_back(p);
                }
                return true;
            });
        }

        return visited_positions;
    }

    // checks wether all cells that match the predicate
    // and are connected to the start position are part of a single connected component
    template <typename Func>
    requires std::predicate<Func, T&, pos2_size>
    bool is_connected(Func predicate, bool orthogonal = true) {
        pos2_size start = pos2_size::invalid();
        size_t total_matching = 0;

        for (size_t y = 0; y < height; y++) {
            for (size_t x = 0; x < width; x++) {
                if (predicate((*this)[y][x], {x, y})) {
                    if (start.x == pos2_size::invalid_value) { start = {x, y}; }
                    total_matching++;
                }
            }
        }

        if (total_matching == 0) { return true; }

        auto visited = _flood_fill_core(start, predicate, orthogonal);
        return visited.size() == total_matching;
    }

    // performs a flood fill starting from position start, filling all connected cells that match the predicate
    template <typename Func>
    requires std::predicate<Func, T&, pos2_size>
    void flood_fill(pos2_size start, Func predicate, bool orthogonal = true) {
        _flood_fill_core(start, predicate, orthogonal);
    }

    // performs a flood fill starting from position start
    // applying the transform function to all connected cells that match the predicate
    template <typename Predicate, typename Transform>
    requires std::predicate<Predicate, T&, pos2_size> && std::invocable<Transform, T&, pos2_size>
    void flood_fill_transform(pos2_size start,
        Predicate predicate,
        Transform transform,
        bool orthogonal = true) {
        auto visited = _flood_fill_core(start, predicate, orthogonal);
        for (const auto& p : visited) {
            transform((*this)[p.y][p.x], p);
        }
    }

    // performs a flood fill starting from position start
    // returns the positions of all connected cells that match the predicate
    template <typename Func>
    requires std::predicate<Func, T&, pos2_size>
    auto get_connected_component(pos2_size start, Func predicate, bool orthogonal = true) {
        return _flood_fill_core(start, predicate, orthogonal);
    }

    // resets the grid to the default value, optionally specifying a default value
    void reset(T def_val = T{}) { data.assign(width * height, def_val); }

    // resizes the grid to new dimensions, optionally specifying a default value for new cells
    void resize(size_t new_width, size_t new_height, T def_val = T{}) {
        if (new_width == width && new_height == height) { return; }

        if (opts & GRID_NO_RETAIN_STATE) {
            data.assign(new_width * new_height, def_val);
            width = new_width;
            height = new_height;
            return;
        }

        std::vector<T, Alloc> new_data(new_width * new_height, def_val);

        size_t x_offset = 0;
        size_t y_offset = 0;

        size_t cw = std::min(width, new_width);
        size_t ch = std::min(height, new_height);

        switch (opts & GRID_GROW_MASK) {
            case GRID_GROW_BOTTOM_RIGHT:
                break;
            case GRID_GROW_BOTTOM_LEFT:
                x_offset = (new_width > width) ? new_width - width : 0;
                break;
            case GRID_GROW_TOP_RIGHT:
                y_offset = (new_height > height) ? new_height - height : 0;
                break;
            case GRID_GROW_TOP_LEFT:
                x_offset = (new_width > width) ? new_width - width : 0;
                y_offset = (new_height > height) ? new_height - height : 0;
                break;
            case GRID_GROW_OUTWARD:
                x_offset = (new_width > width) ? (new_width - width) / 2 : 0;
                y_offset = (new_height > height) ? (new_height - height) / 2 : 0;
                break;
            default:
                ktl_assert(!"invalid grid growth strategy");
                return;
        }

        for (size_t y = 0; y < ch; ++y) {
            std::copy_n(&data[y * width], cw, &new_data[(y + y_offset) * new_width + x_offset]);
        }

        data = std::move(new_data);
        width = new_width;
        height = new_height;
    }

    struct cell {
        T& value;
        pos2_size position;
    };

    auto items() -> std::generator<cell> {
        for (size_t y = 0; y < height; ++y) {
            for (size_t x = 0; x < width; ++x) {
                co_yield cell{data[y * width + x], {x, y}};
            }
        }
    }

    struct const_cell {
        const T& value;
        pos2_size position;
    };

    auto items() const -> std::generator<const_cell> {
        for (size_t y = 0; y < height; ++y) {
            for (size_t x = 0; x < width; ++x) {
                co_yield const_cell{data[y * width + x], {x, y}};
            }
        }
    }

    bool _check_opts() const { return std::popcount(opts & GRID_GROW_MASK) == 1; }
};

}  // namespace ktl_ns

#endif /* GRID_H */
