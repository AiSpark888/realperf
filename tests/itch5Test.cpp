#include "realperf/itch5.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <sstream>
#include <vector>

namespace {

template <typename MessageT>
const MessageT& as_message(const std::vector<std::uint8_t>& bytes)
{
    return *reinterpret_cast<const MessageT*>(bytes.data());
}

std::string print_message(const std::vector<std::uint8_t>& bytes)
{
    const auto message = realperf::itch5::parse_message(bytes.data(), bytes.size());
    std::ostringstream out;
    out << message;
    return out.str();
}

} // namespace

TEST_CASE("ITCH5 make_system_event writes wire bytes")
{
    const auto bytes = realperf::itch5::make_system_event(0x1234, 0x5678, 0x010203040506ULL, 'O');

    const std::vector<std::uint8_t> expected{
        'S',
        0x12, 0x34,
        0x56, 0x78,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        'O',
    };
    CHECK(bytes == expected);

    const auto message = realperf::itch5::parse_message(bytes.data(), bytes.size());
    REQUIRE(message.header != nullptr);
    CHECK(message.extra_bytes == 0);
    CHECK(message.header->type == 'S');

    const auto& event = as_message<realperf::itch5::SystemEvent>(bytes);
    CHECK(realperf::itch5::read_be(event.locate) == 0x1234);
    CHECK(realperf::itch5::read_be(event.tracking) == 0x5678);
    CHECK(realperf::itch5::read_timestamp(event.timestamp) == 0x010203040506ULL);
    CHECK(event.event_code == 'O');

    CHECK(print_message(bytes) == "type=S locate=4660 tracking=22136 ts_ns=1108152157446 system_event=O");
}

TEST_CASE("ITCH5 make_add_order pads symbols and prints decoded fields")
{
    const auto bytes = realperf::itch5::make_add_order(
        1, 2, 9'500'000'000ULL, 0x0102030405060708ULL, 'B', 1'000, "ABC", 123'4500);

    REQUIRE(bytes.size() == realperf::itch5::kAddOrderSize);
    CHECK(bytes[0] == 'A');

    const auto& add = as_message<realperf::itch5::OrderAdd>(bytes);
    CHECK(realperf::itch5::read_be(add.locate) == 1);
    CHECK(realperf::itch5::read_be(add.tracking) == 2);
    CHECK(realperf::itch5::read_timestamp(add.timestamp) == 9'500'000'000ULL);
    CHECK(realperf::itch5::read_be(add.order_ref) == 0x0102030405060708ULL);
    CHECK(add.side == 'B');
    CHECK(realperf::itch5::read_be(add.shares) == 1'000);
    CHECK(std::string_view(add.stock, realperf::itch5::kSymbolSize) == "ABC     ");
    CHECK(realperf::itch5::read_be(add.price) == 123'4500);

    CHECK(print_message(bytes)
          == "type=A locate=1 tracking=2 ts_ns=9500000000 add_order ref=72623859790382856 side=B shares=1000 stock=ABC price=123.4500");
}

TEST_CASE("ITCH5 execution and delete generators write direct-cast messages")
{
    const auto executed_bytes = realperf::itch5::make_order_executed(
        3, 4, 9'500'001'000ULL, 10'000, 500, 99'000);
    REQUIRE(executed_bytes.size() == realperf::itch5::kOrderExecutedSize);

    const auto& executed = as_message<realperf::itch5::OrderExecute>(executed_bytes);
    CHECK(executed.type == 'E');
    CHECK(realperf::itch5::read_be(executed.locate) == 3);
    CHECK(realperf::itch5::read_be(executed.tracking) == 4);
    CHECK(realperf::itch5::read_timestamp(executed.timestamp) == 9'500'001'000ULL);
    CHECK(realperf::itch5::read_be(executed.order_ref) == 10'000);
    CHECK(realperf::itch5::read_be(executed.shares) == 500);
    CHECK(realperf::itch5::read_be(executed.match) == 99'000);
    CHECK(print_message(executed_bytes)
          == "type=E locate=3 tracking=4 ts_ns=9500001000 executed ref=10000 shares=500 match=99000");

    const auto deleted_bytes = realperf::itch5::make_order_delete(5, 6, 9'500'002'000ULL, 11'000);
    REQUIRE(deleted_bytes.size() == realperf::itch5::kOrderDeleteSize);

    const auto& deleted = as_message<realperf::itch5::OrderDelete>(deleted_bytes);
    CHECK(deleted.type == 'D');
    CHECK(realperf::itch5::read_be(deleted.locate) == 5);
    CHECK(realperf::itch5::read_be(deleted.tracking) == 6);
    CHECK(realperf::itch5::read_timestamp(deleted.timestamp) == 9'500'002'000ULL);
    CHECK(realperf::itch5::read_be(deleted.order_ref) == 11'000);
    CHECK(print_message(deleted_bytes) == "type=D locate=5 tracking=6 ts_ns=9500002000 delete ref=11000");
}

TEST_CASE("ITCH5 make_trade writes printable trade messages")
{
    const auto bytes = realperf::itch5::make_trade(
        7, 8, 9'500'003'000ULL, 12'000, 'S', 200, "REALPERF", 10'3368, 77'000);

    REQUIRE(bytes.size() == realperf::itch5::kTradeSize);

    const auto& trade = as_message<realperf::itch5::Trade>(bytes);
    CHECK(trade.type == 'P');
    CHECK(realperf::itch5::read_be(trade.locate) == 7);
    CHECK(realperf::itch5::read_be(trade.tracking) == 8);
    CHECK(realperf::itch5::read_timestamp(trade.timestamp) == 9'500'003'000ULL);
    CHECK(realperf::itch5::read_be(trade.order_ref) == 12'000);
    CHECK(trade.side == 'S');
    CHECK(realperf::itch5::read_be(trade.shares) == 200);
    CHECK(std::string_view(trade.stock, realperf::itch5::kSymbolSize) == "REALPERF");
    CHECK(realperf::itch5::read_be(trade.price) == 10'3368);
    CHECK(realperf::itch5::read_be(trade.match) == 77'000);

    CHECK(print_message(bytes)
          == "type=P locate=7 tracking=8 ts_ns=9500003000 trade ref=12000 side=S shares=200 stock=REALPERF price=10.3368 match=77000");
}

TEST_CASE("ITCH5 parse_message reports extra bytes and rejects truncation")
{
    auto bytes = realperf::itch5::make_system_event(1, 2, 3, 'C');
    bytes.push_back(0xff);
    bytes.push_back(0xee);

    const auto message = realperf::itch5::parse_message(bytes.data(), bytes.size());
    CHECK(message.extra_bytes == 2);

    bytes.resize(realperf::itch5::kSystemEventSize - 1);
    CHECK_THROWS_AS(realperf::itch5::parse_message(bytes.data(), bytes.size()), std::runtime_error);
}
