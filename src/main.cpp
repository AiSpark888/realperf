#include "realperf/perf.hpp"

#include <iostream>

int main()
{
    std::cout << "realperf metric: "
              << realperf::metric_name(realperf::Metric::Latency)
              << '\n';
    return 0;
}
