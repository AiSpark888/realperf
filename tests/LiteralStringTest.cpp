#include "realperf/LiteralString.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <sstream>

REALPERF_LITERAL_STRING(latency_literal, "latency")
REALPERF_LITERAL_STRING(throughput_literal, "throughput")

realperf::LiteralString external_literal_string();

TEST_CASE("LiteralString stores string literal identity in a uint32_t")
{
    constexpr realperf::LiteralString latency{"latency"};
    constexpr realperf::LiteralString same_latency{"latency"};
    constexpr realperf::LiteralString throughput{"throughput"};

    static_assert(sizeof(realperf::LiteralString) == sizeof(std::uint32_t));
    static_assert(latency == same_latency);
    static_assert(latency != throughput);
    static_assert(!latency.empty());
    static_assert(realperf::LiteralString{}.empty());

    CHECK(latency == same_latency);
    CHECK(latency != throughput);
    CHECK_FALSE(latency.empty());
    CHECK(realperf::LiteralString{}.empty());
}

TEST_CASE("LiteralString can be printed through the linked literal string table")
{
    constexpr realperf::LiteralString latency{"latency"};
    constexpr realperf::LiteralString missing{"missing"};

    CHECK(realperf::literal_string_text(latency) == "latency");
    CHECK(realperf::literal_string_text(external_literal_string()) == "external");
    CHECK(realperf::literal_string_text(missing).empty());

    std::ostringstream output;
    output << latency;

    CHECK(output.str() == "latency");
}
