#include "realperf/perf.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("metric_name returns enum names")
{
    CHECK(realperf::metric_name(realperf::Metric::Latency) == "Latency");
    CHECK(realperf::metric_name(realperf::Metric::Throughput) == "Throughput");
    CHECK(realperf::metric_name(realperf::Metric::CpuUsage) == "CpuUsage");
}
