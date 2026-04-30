#include "realperf/LiteralString.hpp"

REALPERF_LITERAL_STRING(external_literal, "external")

realperf::LiteralString external_literal_string()
{
    return external_literal;
}
