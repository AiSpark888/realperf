#include "realperf/LiteralString.hpp"

#include <cstddef>
#include <string_view>

extern "C" {
extern const realperf::LiteralStringEntry __start_realperf_literal_strings[] __attribute__((weak));
extern const realperf::LiteralStringEntry __stop_realperf_literal_strings[] __attribute__((weak));
}

namespace realperf {

std::span<const LiteralStringEntry> literal_string_entries()
{
    if (__start_realperf_literal_strings == nullptr || __stop_realperf_literal_strings == nullptr) {
        return {};
    }

    return {
        __start_realperf_literal_strings,
        static_cast<std::size_t>(__stop_realperf_literal_strings - __start_realperf_literal_strings)
    };
}

std::string_view literal_string_text(LiteralString literal)
{
    for (const LiteralStringEntry& entry : literal_string_entries()) {
        if (entry.literal == literal) {
            return entry.text;
        }
    }

    return {};
}

void LiteralString::dumpLiteralStrings(std::ostream& out)
{
    out << "literal,fingerprint\n";
    for (const LiteralStringEntry& entry : literal_string_entries()) {
        out << entry.text << ',' << entry.literal.value() << '\n';
    }
}

std::ostream& operator<<(std::ostream& output, LiteralString literal)
{
    return output << literal_string_text(literal);
}

} // namespace realperf
