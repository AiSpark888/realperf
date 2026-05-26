#include "realperf/DoubleMapBuffer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <cstddef>
#include <stdexcept>

#include <unistd.h>

namespace {

std::size_t pageSize()
{
    const long value = ::sysconf(_SC_PAGESIZE);
    REQUIRE(value > 0);
    return static_cast<std::size_t>(value);
}

} // namespace

TEST_CASE("DoubleMapBuffer rounds sizes up to the system page size")
{
    const std::size_t page = pageSize();

    CHECK(realperf::DoubleMapBuffer::roundUpToPageSize(1u) == page);
    CHECK(realperf::DoubleMapBuffer::roundUpToPageSize(page) == page);
    CHECK(realperf::DoubleMapBuffer::roundUpToPageSize(page + 1u) == page * 2u);
}

TEST_CASE("DoubleMapBuffer rounds sizes up to a power of two")
{
    CHECK(realperf::DoubleMapBuffer::roundUpToPowerOfTwo(1u) == 1u);
    CHECK(realperf::DoubleMapBuffer::roundUpToPowerOfTwo(2u) == 2u);
    CHECK(realperf::DoubleMapBuffer::roundUpToPowerOfTwo(3u) == 4u);
    CHECK(realperf::DoubleMapBuffer::roundUpToPowerOfTwo(9u) == 16u);
}

TEST_CASE("DoubleMapBuffer requires a non-zero capacity")
{
    CHECK_THROWS_AS(realperf::DoubleMapBuffer(0u), std::invalid_argument);
}

TEST_CASE("DoubleMapBuffer maps its storage twice back-to-back")
{
    const std::size_t page = pageSize();
    realperf::DoubleMapBuffer buffer(page + 1u);

    REQUIRE(buffer.capacity() == page * 2u);
    REQUIRE(std::has_single_bit(buffer.capacity()));
    REQUIRE(buffer.buffer() != nullptr);

    auto* data = static_cast<std::byte*>(buffer.buffer());

    data[buffer.capacity() - 2u] = std::byte {0x10};
    data[buffer.capacity() - 1u] = std::byte {0x20};
    data[buffer.capacity()] = std::byte {0x30};
    data[buffer.capacity() + 1u] = std::byte {0x40};

    CHECK(data[buffer.capacity() - 2u] == std::byte {0x10});
    CHECK(data[buffer.capacity() - 1u] == std::byte {0x20});
    CHECK(data[0] == std::byte {0x30});
    CHECK(data[1] == std::byte {0x40});
    CHECK(data[buffer.capacity()] == data[0]);
    CHECK(data[buffer.capacity() + 1u] == data[1]);
}
