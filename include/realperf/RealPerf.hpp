#pragma once

#include "realperf/CheckPoints.hpp"

#ifndef REALPERF_CHECKPOINT // bitmap of enabled checkpoint categories
#define REALPERF_CHECKPOINT 0ull; //disable all checkpoints by default
#endif

// example usage:
// RP_CHECKPOINT("my_function"); // creates a checkpoint

namespace realperf {

void checkpoint( LiteralString where, CheckPoint::Type type = CheckPoint::Type::Normal, Core );
}

