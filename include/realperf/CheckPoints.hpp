#pragma once

#include <LiteralString.hpp>
#include <Defines.hpp>

namespace realperf {

struct CheckPoint
{
    enum class Type : uint8_t {
        Start,  // start event
        Normal, // normal checkpoint
        ScopeStart, // start of a scope
        ScopeEnd,   // end of a scope
        Order,  // order event, variable length
        End,    // end event
        Custom = 100, // custom event, variable length

    };

    enum class Flags {
        Core,       // utilities, platform generic code
        MarketData, // this is a market data event
        Signal,     // this is a signal event
        Order,      // an order event occurred
        Custom,
    };

    Tick tick;
    LiteralString where;
    Type type : 8;
    uint8_t length;
    uint16_t reserved;
};
static_assert(sizeof(CheckPoint) == 16, "CheckPoint must be 16 bytes in size");

// in my opinion this should be a runtime check
#ifndef REALPERF_CHECKPOINT // bitmap of enabled checkpoint categories
#define REALPERF_CHECKPOINT 0ull; //disable all checkpoints by default
#endif


} // namespace realperf