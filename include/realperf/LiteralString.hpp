#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <span>
#include <string_view>

namespace realperf {

class LiteralString {
public:
    consteval LiteralString() = default;

    template <std::size_t Size>
    consteval explicit LiteralString(const char (&literal)[Size])
        : LiteralString(fingerprint(literal))
    {
    }

    template <std::size_t Size>
    static consteval LiteralString fromLiteral(const char (&literal)[Size])
    {
        return LiteralString {literal};
    }

    constexpr std::uint32_t value() const
    {
        return fingerprint_;
    }

    constexpr bool empty() const
    {
        return value() == 0u;
    }

    static void dumpLiteralStrings(std::ostream& out);

    friend constexpr bool operator==(LiteralString left, LiteralString right) = default;

    friend constexpr auto operator<=>(LiteralString left, LiteralString right)
    {
        return left.value() <=> right.value();
    }

private:
    consteval explicit LiteralString(std::uint32_t value)
        : fingerprint_(value)
    {
    }

    template <std::size_t Size>
    static consteval std::uint32_t fingerprint(const char (&literal)[Size])
    {
        std::uint32_t hash = 0x811c9dc5u;
        for (std::size_t index = 0; index < Size - 1u; ++index) {
            hash ^= static_cast<unsigned char>(literal[index]);
            hash *= 0x01000193u;
        }

        const std::uint32_t value = hash & 0x00ff'ffffu;
        return value == 0u ? 1u : value;
    }

    std::uint32_t fingerprint_ {};
};

static_assert(sizeof(LiteralString) == sizeof(std::uint32_t));

struct alignas(16) LiteralStringEntry {
    template <std::size_t Size>
    consteval explicit LiteralStringEntry(const char (&literalText)[Size])
        : literal(LiteralString::fromLiteral(literalText))
        , text(literalText, Size - 1u)
    {
    }

    LiteralString literal;
    std::string_view text;
};

std::span<const LiteralStringEntry> literal_string_entries();
std::string_view literal_string_text(LiteralString literal);
std::ostream& operator<<(std::ostream& output, LiteralString literal);

#if defined(__GNUC__) || defined(__clang__)
#define REALPERF_LITERAL_STRING(name, literal_text)                         \
    inline constexpr ::realperf::LiteralString name {literal_text};         \
    namespace {                                                             \
    [[gnu::used, gnu::section("realperf_literal_strings")]]                \
    constinit const ::realperf::LiteralStringEntry name##_entry {           \
        literal_text                                                        \
    };                                                                      \
    }
#else
#error "REALPERF_LITERAL_STRING currently requires a GNU-compatible linker section implementation"
#endif

} // namespace realperf
