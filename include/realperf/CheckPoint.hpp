#pragma once
#include <cstdint>
#include "LiteralString.hpp"

namespace realperf {

struct CheckPoint
{
    enum class Type : std::uint8_t
    {
        CP_Start,
        CP_Normal,
        CP_ScopeStart,
        CP_ScopeEnd,
        CP_Order,
        CP_Event,
        CP_Custom,
        CP_End,
    };

    enum Category : std::uint8_t
    {
        CAT_Default = 0,
        CAT_MD = 1,
        CAT_Order = 2,
        CAT_Strategy = 3,
    };

    using Tick = std::uint64_t;
    Tick tick_;
    LiteralString where_;
    Type type_;
    Category category_;
};

struct CheckPointStart
{

};
}