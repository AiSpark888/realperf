#pragma once

#include <bit>
#include <cstddef>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace realperf {

struct DoubleMapBuffer
{
    static size_t roundUpToPageSize(size_t size)
    {
        const long pageSize = ::sysconf(_SC_PAGESIZE);
        if (pageSize <= 0) {
            throw std::system_error(errno, std::generic_category(), "sysconf(_SC_PAGESIZE)");
        }

        return ((size + pageSize - 1u) / pageSize) * pageSize;
    }

    static size_t roundUpToPowerOfTwo(size_t size)
    {
        if (!std::has_single_bit(size)) {
            size = std::bit_ceil(size);
        }
        return size;
    }

    DoubleMapBuffer(std::size_t capacity)
    {
        if (capacity == 0u) {
            throw std::invalid_argument("DoubleMapBuffer capacity must not be zero");
        }

        capacity_ = roundUpToPageSize(capacity);
        capacity_ = roundUpToPowerOfTwo(capacity_);

        fd_ = createBackingFile(capacity_);
        try {
            mapTwice();
        } catch (...) {
            release();
            throw;
        }
    }

    DoubleMapBuffer(const DoubleMapBuffer&) = delete;
    DoubleMapBuffer& operator=(const DoubleMapBuffer&) = delete;

    DoubleMapBuffer(DoubleMapBuffer&& other) noexcept
    {
        moveFrom(std::move(other));
    }

    DoubleMapBuffer& operator=(DoubleMapBuffer&& other) noexcept
    {
        if (this != &other) {
            release();
            moveFrom(std::move(other));
        }

        return *this;
    }

    ~DoubleMapBuffer()
    {
        release();
    }

    void * buffer() const
    {
        return buffer_;
    }

    size_t capacity() const
    {
        return capacity_;
    }
    
private:
    static void throwSystemError(const char* operation)
    {
        throw std::runtime_error(
            std::string(operation) + " failed: " + std::strerror(errno));
    }

    static int createBackingFile(std::size_t capacity)
    {
        char path[] = "/tmp/realperf-double-map-buffer-XXXXXX";
        const int fd = ::mkstemp(path);
        if (fd == -1) {
            throwSystemError("mkstemp");
        }

        ::unlink(path);

        if (::ftruncate(fd, static_cast<off_t>(capacity)) == -1) {
            const int savedErrno = errno;
            ::close(fd);
            errno = savedErrno;
            throwSystemError("ftruncate");
        }

        return fd;
    }

    void mapTwice()
    {
        if (capacity_ > std::numeric_limits<std::size_t>::max() / 2u) {
            throw std::overflow_error("DoubleMapBuffer mirrored capacity overflows size_t");
        }

        void* reservation = ::mmap(
            nullptr,
            capacity_ * 2u,
            PROT_NONE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1,
            0);
        if (reservation == MAP_FAILED) {
            throwSystemError("mmap reservation");
        }

        void* first = ::mmap(
            reservation,
            capacity_,
            PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_FIXED,
            fd_,
            0);
        if (first == MAP_FAILED) {
            const int savedErrno = errno;
            ::munmap(reservation, capacity_ * 2u);
            errno = savedErrno;
            throwSystemError("mmap first view");
        }

        void* secondAddress = static_cast<std::byte*>(reservation) + capacity_;
        void* second = ::mmap(
            secondAddress,
            capacity_,
            PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_FIXED,
            fd_,
            0);
        if (second == MAP_FAILED) {
            const int savedErrno = errno;
            ::munmap(reservation, capacity_ * 2u);
            errno = savedErrno;
            throwSystemError("mmap second view");
        }

        buffer_ = reservation;
    }

    void moveFrom(DoubleMapBuffer&& other) noexcept
    {
        buffer_ = std::exchange(other.buffer_, nullptr);
        capacity_ = std::exchange(other.capacity_, 0u);
        fd_ = std::exchange(other.fd_, -1);
    }

    void release() noexcept
    {
        if (buffer_ != nullptr) {
            ::munmap(buffer_, capacity_ * 2u);
        }

        if (fd_ != -1) {
            ::close(fd_);
        }

        buffer_ = nullptr;
        capacity_ = 0u;
        fd_ = -1;
    }

    void * buffer_ = nullptr;
    std::size_t capacity_ = 0;
    int fd_ = -1;
};
}
