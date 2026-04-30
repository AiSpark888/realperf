#include "realperf/perf.hpp"

#include <magic_enum/magic_enum.hpp>

namespace realperf {

std::string_view metric_name(Metric metric)
{
    return magic_enum::enum_name(metric);
}

} // namespace realperf
