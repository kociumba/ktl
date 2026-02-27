#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

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
        REQUIRE(!err);
        std::printf("%d\n", val);
    }

    SECTION("check both returns") {
        const auto& [val, err] = return_both();
        REQUIRE(err);
        std::printf("val: %d, err: %s\n", val, *err);
    }
}