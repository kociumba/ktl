#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#define KTL_RESULT_FUNCTIONAL_EXTENSIONS
#include <result.h>

#include <print>
#include <string>

using namespace ktl;

result<int, const char*> return_only_err() { return err("error message"); }
result<int, const char*> return_only_val() { return 69; }
result<int, const char*> return_both() { return {420, "non fatal error message"}; }

TEST_CASE("basic functionality", "[result]") {
    SECTION("check error return") {
        const auto& [_, err] = return_only_err();
        REQUIRE(err);
        if (err) { std::printf("%s\n", *err); }
    }

    SECTION("check value return") {
        const auto& [val, err] = return_only_val();
        REQUIRE_FALSE(err);
        std::printf("%d\n", val);
    }

    SECTION("check both returns") {
        const auto& [val, err] = return_both();
        REQUIRE(err);
        std::printf("val: %d, err: %s\n", val, *err);
    }
}

TEST_CASE("functional extension", "[result]") {
    SECTION("map transforms value when !err") {
        const auto& [val, err] = return_only_val().map([](int v) { return v * 2; });

        REQUIRE_FALSE(err);
        REQUIRE(val == 138);
    }

    SECTION("map propagates error") {
        const auto& [_, err] = return_only_err().map([](int v) { return v * 2; });
        REQUIRE(err);
    }

    SECTION("and_then chains successful results") {
        const auto& [val, err] =
            return_only_val().and_then([](int v) -> result<int, const char*> { return v + 1; });

        REQUIRE_FALSE(err);
        REQUIRE(val == 70);
    }

    SECTION("and_then short circuits on error") {
        bool called = false;
        auto res = return_only_err().and_then([&](int v) -> result<int, const char*> {
            called = true;
            return v + 1;
        });

        REQUIRE_FALSE(called);
        REQUIRE(res.err);
    }

    SECTION("or_else recovers from error") {
        const auto& [val, err] =
            return_only_err().or_else([](const char*) -> result<int, const char*> { return 0; });

        REQUIRE_FALSE(err);
        REQUIRE(val == 0);
    }

    SECTION("or_else passes through on ok") {
        bool called = false;
        auto res = return_only_val().or_else([&](const char*) -> result<int, const char*> {
            called = true;
            return 0;
        });

        REQUIRE_FALSE(called);
        REQUIRE(res.val == 69);
    }

    SECTION("value_or returns value when ok") { REQUIRE(return_only_val().value_or(0) == 69); }

    SECTION("value_or returns fallback on error") {
        REQUIRE(return_only_err().value_or(420) == 420);
    }

    SECTION("map and and_then can be chained") {
        const auto& [val, err] =
            return_only_val()
                .map([](int v) { return v + 1; })
                .and_then([](int v) -> result<int, const char*> { return v * 2; });

        REQUIRE_FALSE(err);
        REQUIRE(val == 140);
    }
}