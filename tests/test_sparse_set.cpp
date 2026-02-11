#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <sparse_set.h>

#include <algorithm>
#include <numeric>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

using namespace ktl;

TEST_CASE("Sparse set basic operations", "[sparse_set]") {
    sparse_set<std::string> set;

    auto id1 = set.emplace_back("first");
    auto id2 = set.emplace_back("second");
    auto id3 = set.emplace_back("third");

    REQUIRE(set.size() == 3);
    REQUIRE(set[id1] == "first");
    REQUIRE(set[id2] == "second");

    SECTION("Erase middle element") {
        set.erase(id2);
        REQUIRE(set.size() == 2);
        REQUIRE(set[id1] == "first");
        REQUIRE(set[id3] == "third");
    }

    SECTION("Add after erase reuses slot") {
        set.erase(id2);
        auto id4 = set.emplace_back("fourth");
        REQUIRE(set.size() == 3);
        REQUIRE(set[id4] == "fourth");

        // Old id2 points to same slot, different validity
        REQUIRE(set[id2] == "fourth");
    }
}

TEST_CASE("Sparse set handles", "[sparse_set]") {
    sparse_set<std::string> set;

    auto id1 = set.emplace_back("first");
    auto id2 = set.emplace_back("second");

    auto h1 = set.create_handle(id1);
    auto h2 = set.create_handle(id2);

    REQUIRE(h1.valid());
    REQUIRE(*h1 == "first");

    SECTION("Handle modification") {
        h1->append(" modified");
        REQUIRE(set[id1] == "first modified");
    }

    SECTION("Handle invalidation on erase") {
        set.erase(id2);
        REQUIRE_FALSE(h2.valid());
        REQUIRE(h1.valid());
    }
}

TEST_CASE("Sparse set stress test", "[sparse_set]") {
    sparse_set<int> set;
    std::vector<basic_handle<int>> handles;

    // Add 1000 elements
    for (int i = 0; i < 1000; ++i) {
        handles.emplace_back(set.create_handle(set.emplace_back(i)));
    }

    // Erase even indices
    for (size_t i = 0; i < handles.size(); i += 2) {
        if (handles[i].valid()) { set.erase(handles[i]); }
    }

    size_t valid_count = 0;
    for (auto& h : handles) {
        if (h.valid()) ++valid_count;
    }
    REQUIRE(valid_count == 500);
}

TEST_CASE("Sparse set remove_if", "[sparse_set]") {
    sparse_set<int> set;
    set.emplace_back(1);
    set.emplace_back(2);
    set.emplace_back(3);
    set.emplace_back(4);

    set.remove_if([](const int& v) { return v % 2 == 0; });

    REQUIRE(set.size() == 2);

    bool has_even = false;
    for (const auto& v : set) {
        if (v % 2 == 0) has_even = true;
    }
    REQUIRE_FALSE(has_even);
}

TEST_CASE("Sparse set clear", "[sparse_set]") {
    sparse_set<int> set;
    auto id = set.emplace_back(42);
    auto h = set.create_handle(id);

    REQUIRE(h.valid());

    set.clear();

    REQUIRE(set.empty());
    REQUIRE_FALSE(h.valid());
}

TEST_CASE("Sparse set benchmarks", "[sparse_set][!benchmark]") {
    static constexpr size_t N = 100'000;
    static constexpr size_t NumAccess = N * 10;
    static constexpr size_t NumErase = N / 10000;

    BENCHMARK_ADVANCED("ktl::sparse_set " + std::to_string(N) + " elements and " +
                       std::to_string(NumErase) + " random elements removed")
    (Catch::Benchmark::Chronometer meter) {
        sparse_set<int> set;
        set.reserve(N);
        for (size_t i = 0; i < N; ++i) {
            set.emplace_back(static_cast<int>(i));
        }

        std::mt19937 rng(std::random_device{}());

        meter.measure([&](int iter) {
            if (iter - 2 <= set.size()) { return (size_t)0; }
            for (size_t i = 0; i < NumErase; ++i) {
                auto upper = set.size() - i;
                std::uniform_int_distribution<size_t> dist(1, upper <= 1 ? 2 : upper);

                set.erase_via_data(dist(rng));
            }
            return set.size();
        });
    };

    BENCHMARK_ADVANCED("std::vector " + std::to_string(N) + " elements and " +
                       std::to_string(NumErase) + " random elements removed")
    (Catch::Benchmark::Chronometer meter) {
        std::vector<int> vec;
        vec.reserve(N);
        for (size_t i = 0; i < N; ++i) {
            vec.emplace_back(static_cast<int>(i));
        }

        std::mt19937 rng(std::random_device{}());

        meter.measure([&] {
            for (size_t i = 0; i < NumErase; ++i) {
                std::uniform_int_distribution<size_t> dist(1, vec.size() - i);

                vec.erase(vec.begin() + dist(rng));
            }
            return vec.size();
        });
    };

    BENCHMARK_ADVANCED("ktl::sparse_set  iterate over " + std::to_string(N) + " elements")
    (Catch::Benchmark::Chronometer meter) {
        sparse_set<int> set;
        set.reserve(N);
        for (size_t i = 0; i < N; ++i) {
            set.emplace_back(static_cast<int>(i));
        }

        std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<size_t> dist(1, set.size());

        meter.measure([&] {
            for (auto& elem : set) {
                (void)(elem + dist(rng));
            }
            return set.size();
        });
    };

    BENCHMARK_ADVANCED("std::unordered_map  iterate over " + std::to_string(N) + " elements")
    (Catch::Benchmark::Chronometer meter) {
        std::unordered_map<int, int> map;
        map.reserve(N);
        for (size_t i = 0; i < N; ++i) {
            map[(int)i] = 0;
        }

        std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<size_t> dist(1, map.size());

        meter.measure([&] {
            for (auto& [key, val] : map) {
                (void)(key + dist(rng));
            }
            return map.size();
        });
    };
}
