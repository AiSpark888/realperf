#pragma once

#include "realperf/LiteralString.hpp"

#include <string_view>

namespace realperf {

enum class Metric {
    Latency,
    Throughput,
    CpuUsage,
};

std::string_view metric_name(Metric metric);

} // namespace realperf
