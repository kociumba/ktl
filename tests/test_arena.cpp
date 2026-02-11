#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <arena.h>

#include <string>

using namespace ktl;

TEST_CASE("Arena basic allocation and snapshot", "[arena]") {
    Arena arena;

    SECTION("Basic allocation") {
        void* p1 = arena_alloc(&arena, 100);
        REQUIRE(p1 != nullptr);
    }

    SECTION("Snapshot and rewind") {
        void* p1 = arena_alloc(&arena, 100);
        REQUIRE(p1 != nullptr);

        ArenaSnapshot snap = arena_snapshot(&arena);
        void* p2 = arena_alloc(&arena, 200);
        REQUIRE(p2 != nullptr);

        arena_rewind(&arena, snap);
        void* p3 = arena_alloc(&arena, 100);  // reuses p2's space
        REQUIRE(p3 == p2);
    }

    arena_free(&arena);
}

TEST_CASE("Arena with C++ objects", "[arena]") {
    Arena arena;

    std::string* s1 = new (arena_alloc(&arena, sizeof(std::string))) std::string("hello");
    REQUIRE(*s1 == "hello");

    std::string* s2 = new (arena_alloc(&arena, sizeof(std::string))) std::string("world");
    REQUIRE(*s2 == "world");

    SECTION("Snapshot preserves earlier objects") {
        ArenaSnapshot snap = arena_snapshot(&arena);
        arena_rewind(&arena, snap);

        std::string* s3 = new (arena_alloc(&arena, sizeof(std::string))) std::string("again");
        REQUIRE(*s3 == "again");
        REQUIRE(*s1 == "hello");  // s1 still valid
    }

    arena_free(&arena);
}

TEST_CASE("Arena large allocation", "[arena]") {
    Arena arena;

    // Test allocation that forces new region
    size_t large = KTL_ARENA_REGION_DEFAULT_CAPACITY * sizeof(uintptr_t) + 1000;
    void* p = arena_alloc(&arena, large);
    REQUIRE(p != nullptr);

    arena_free(&arena);
}

TEST_CASE("Arena benchmarks", "[arena][!benchmark]") {
    BENCHMARK("Arena allocation - small objects") {
        Arena arena;
        for (int i = 0; i < 1000; ++i) {
            arena_alloc(&arena, 64);
        }
        auto result = arena.region_creations;
        arena_free(&arena);
        return result;
    };

    BENCHMARK_ADVANCED("Arena allocation - with snapshots")(Catch::Benchmark::Chronometer meter) {
        Arena arena;

        meter.measure([&] {
            for (int i = 0; i < 100; ++i) {
                ArenaSnapshot snap = arena_snapshot(&arena);
                arena_alloc(&arena, 128);
                arena_rewind(&arena, snap);
            }
            return arena.region_creations;
        });

        arena_free(&arena);
    };
}