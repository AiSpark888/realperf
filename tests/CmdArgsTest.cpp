#include "realperf/CmdArgs.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

realperf::CmdArgs make_args(std::initializer_list<const char*> values)
{
    auto argv = std::vector<char*>{};
    argv.reserve(values.size());
    for (const char* value : values) {
        argv.push_back(const_cast<char*>(value));
    }
    return realperf::CmdArgs(static_cast<int>(argv.size()), argv.data());
}

} // namespace

TEST_CASE("CmdArgs stores command-line values in a string map", "[CmdArgs]")
{
    const auto args = make_args({"tool", "-n", "42", "--mode=itch5", "input.pcap", "--verbose"});

    REQUIRE(args.values().at("n") == "42");
    REQUIRE(args.values().at("mode") == "itch5");
    REQUIRE(args.values().at("verbose").empty());
    CHECK(!args.has("-n"));
    CHECK(!args.has("--mode"));
    REQUIRE(args.positional_count() == 1);
    CHECK(args.positional(0) == "input.pcap");
}

TEST_CASE("CmdArgs converts values by argument name", "[CmdArgs]")
{
    const auto args = make_args({"tool", "--count", "1000", "--ratio", "2.5", "--name", "REALPERF", "--flag"});

    CHECK(args.as<std::uint32_t>("count") == 1000);
    CHECK(args.as<double>("ratio") == 2.5);
    CHECK(args.as<std::string>("name") == "REALPERF");
    CHECK(args.as<std::string_view>("name") == "REALPERF");
    CHECK(args.as<bool>("flag"));
}

TEST_CASE("CmdArgs reports missing and invalid typed values", "[CmdArgs]")
{
    const auto args = make_args({"tool", "--count", "abc", "--flag"});

    CHECK_THROWS_AS(args.as<std::uint32_t>("count"), std::runtime_error);
    CHECK_THROWS_AS(args.as<std::string>("flag"), std::runtime_error);
    CHECK_THROWS_AS(args.as<std::string>("missing"), std::runtime_error);
}
