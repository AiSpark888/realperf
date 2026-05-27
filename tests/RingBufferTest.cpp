#include "realperf/RingBuffer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

std::size_t pageSize()
{
    const long value = ::sysconf(_SC_PAGESIZE);
    REQUIRE(value > 0);
    return static_cast<std::size_t>(value);
}

} // namespace

TEST_CASE("RingBuffer requires a power-of-two capacity")
{
    CHECK_THROWS_AS(realperf::RingBuffer<std::byte>(0), std::invalid_argument);
    CHECK_THROWS_AS(realperf::RingBuffer<std::byte>(3), std::invalid_argument);
}

TEST_CASE("RingBuffer requires a page-sized backing area")
{
    CHECK_THROWS_AS(realperf::RingBuffer<std::byte>(2), std::invalid_argument);
}

TEST_CASE("RingBuffer maps its storage twice back-to-back")
{
    const std::size_t capacity = pageSize() / sizeof(std::uint32_t);
    REQUIRE(std::has_single_bit(capacity));

    realperf::RingBuffer<std::uint32_t> buffer(capacity);

    REQUIRE(buffer.capacity() == capacity);
    REQUIRE(buffer.byteSize() == pageSize());
    REQUIRE(buffer.data() != nullptr);

    buffer.data()[capacity - 2u] = 10u;
    buffer.data()[capacity - 1u] = 20u;
    buffer.data()[capacity] = 30u;
    buffer.data()[capacity + 1u] = 40u;

    CHECK(buffer.data()[capacity - 2u] == 10u);
    CHECK(buffer.data()[capacity - 1u] == 20u);
    CHECK(buffer.data()[0] == 30u);
    CHECK(buffer.data()[1] == 40u);
    CHECK(buffer.data()[capacity] == buffer.data()[0]);
    CHECK(buffer.data()[capacity + 1u] == buffer.data()[1]);
}

TEST_CASE("RingBuffer indexed access wraps by capacity")
{
    realperf::RingBuffer<std::byte> buffer(pageSize());

    buffer[0] = std::byte {0x11};
    buffer[buffer.capacity() - 1u] = std::byte {0x22};
    buffer[buffer.capacity()] = std::byte {0x33};

    CHECK(buffer[0] == std::byte {0x33});
    CHECK(buffer[buffer.capacity()] == std::byte {0x33});
    CHECK(buffer[buffer.capacity() - 1u] == std::byte {0x22});
}

TEST_CASE("RingBuffer can use a named shared backing file")
{
    const std::size_t capacity = pageSize() / sizeof(std::uint32_t);
    REQUIRE(std::has_single_bit(capacity));

    char path[] = "/tmp/realperf-ring-buffer-test-XXXXXX";
    const int tmp = ::mkstemp(path);
    REQUIRE(tmp != -1);
    REQUIRE(::close(tmp) == 0);
    REQUIRE(::unlink(path) == 0);

    {
        realperf::RingBuffer<std::uint32_t> buffer(capacity, path);

        buffer.data()[0] = 0x12345678u;
        buffer.data()[capacity] = 0x90abcdefu;

        struct stat info {};
        REQUIRE(::stat(path, &info) == 0);
        CHECK(static_cast<std::size_t>(info.st_size) == pageSize());
        CHECK(buffer.data()[0] == 0x90abcdefu);
    }

    const int fd = ::open(path, O_RDONLY);
    REQUIRE(fd != -1);

    std::uint32_t stored {};
    REQUIRE(::read(fd, &stored, sizeof(stored)) == static_cast<ssize_t>(sizeof(stored)));
    CHECK(stored == 0x90abcdefu);

    REQUIRE(::close(fd) == 0);
    REQUIRE(::unlink(path) == 0);
}
