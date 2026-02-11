#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include "common.h"

#include <geometry_primitives.h>
#include <grid.h>

using namespace ktl;

TEST_CASE("Grid basic construction and access", "[grid]") {
    grid<int> g(5, 5, 0);

    SECTION("Access and bounds checking") {
        g.at(2, 2) = 42;
        REQUIRE(g.xy(2, 2) == 42);
        REQUIRE(g.in_bounds({2, 2}));
        REQUIRE_FALSE(g.in_bounds({5, 5}));
    }
}

TEST_CASE("Grid resize with growth strategies", "[grid]") {
    SECTION("GROW_BOTTOM_RIGHT (default)") {
        grid<int> g(3, 3, 1);
        g.at(1, 1) = 999;

        g.resize(5, 5, 0);
        REQUIRE(g.width == 5);
        REQUIRE(g.height == 5);
        REQUIRE(g.at(1, 1) == 999);  // old center preserved
    }

    SECTION("GROW_TOP_LEFT") {
        grid<int> g2(3, 3, 1, GRID_GROW_TOP_LEFT);
        g2.at(1, 1) = 999;
        g2.resize(5, 5, 0);
        REQUIRE(g2.at(3, 3) == 999);  // shifted to bottom-right
    }

    SECTION("GROW_OUTWARD") {
        grid<int> g3(3, 3, 1, GRID_GROW_OUTWARD);
        g3.at(1, 1) = 999;
        g3.resize(5, 5, 0);
        REQUIRE(g3.at(2, 2) == 999);  // center stays roughly in center
    }
}

TEST_CASE("Grid neighbors", "[grid]") {
    grid<int> g(5, 5, 0);

    SECTION("Orthogonal neighbors") {
        int sum_ortho = 0;
        g.orthogonal_neighbors({2, 2}, [&](int& cell, pos2_size) {
            sum_ortho += cell;
            return true;
        });
        REQUIRE(sum_ortho == 0);
    }

    SECTION("Moore neighbors") {
        int count_moore = 0;
        g.moore_neighbors({2, 2}, [&](int&, pos2_size) {
            ++count_moore;
            return true;
        });
        REQUIRE(count_moore == 8);
    }
}

TEST_CASE("Grid flood fill basics", "[grid]") {
    grid<char> g(10, 10, '.');
    for (int i = 0; i < g.width; ++i) {
        g.xy(i, 5) = '#';  // horizontal wall
    }

    test_utils::debug_print_grid(g);

    auto is_open = [](char& c, pos2_size) { return c == '.'; };
    auto component = g.get_connected_component({0, 0}, is_open);

    REQUIRE(component.size() > 0);

    // Should not cross the wall
    bool crossed = false;
    for (auto p : component) {
        if (p.y > 5) crossed = true;
    }
    REQUIRE_FALSE(crossed);
}

TEST_CASE("Grid flood fill transform", "[grid]") {
    grid<int> g(5, 5, 0);
    g.at(2, 2) = 1;

    auto is_one = [](int& v, pos2_size) { return v == 1; };
    g.flood_fill_transform({2, 2}, is_one, [](int& v, pos2_size) { v = 42; }, true);

    REQUIRE(g.at(2, 2) == 42);
    REQUIRE(g.at(0, 0) == 0);  // outside component unchanged
}

TEST_CASE("Grid connected component check", "[grid]") {
    grid<char> g(5, 5, '.');
    g.at(0, 0) = '#';
    g.at(4, 4) = '#';

    test_utils::debug_print_grid(g);

    auto is_wall = [](char& c, pos2_size) { return c == '#'; };
    REQUIRE_FALSE(g.is_connected(is_wall));  // two separate components
}

TEST_CASE("Grid items generator", "[grid]") {
    grid<int> g(3, 3, 0);
    int i = 0;
    for (auto cell : g.items()) {
        cell.value = ++i;
    }
    REQUIRE(g.at(2, 2) == 9);
}

TEST_CASE("Grid benchmarks", "[grid][!benchmark]") {
    BENCHMARK("Grid construction 100x100") { return grid<int>(100, 100, 0); };

    BENCHMARK("Grid flood fill 100x100") {
        grid<int> g(100, 100, 0);
        g.at(50, 50) = 1;

        auto is_one = [](int& v, pos2_size) { return v == 1; };
        return g.get_connected_component({0, 0}, is_one);
    };

    BENCHMARK("Grid neighbor iteration") {
        grid<int> g(100, 100, 0);
        int sum = 0;

        for (int y = 1; y < 99; ++y) {
            for (int x = 1; x < 99; ++x) {
                g.moore_neighbors(x, y, [&](int& cell, pos2_size) {
                    sum += cell;
                    return true;
                });
            }
        }

        return sum;
    };
}