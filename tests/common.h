#ifndef COMMON_H
#define COMMON_H

#include <cstdio>

#include <grid.h>

namespace test_utils {

inline void debug_print_grid(const ktl::grid<char>& g) {
    std::printf("    ");
    for (size_t x = 0; x < g.width; ++x) {
        std::printf("%zu ", x % 10);
    }
    std::printf("\n");

    for (size_t y = 0; y < g.height; ++y) {
        std::printf("%3zu ", y);

        for (size_t x = 0; x < g.width; ++x) {
            char c = g[y][x];

            if (c <= 32) {
                std::printf(". ");
            } else {
                std::printf("%c ", c);
            }
        }
        std::printf("\n");
    }
}

}  // namespace test_utils

#endif /* COMMON_H */