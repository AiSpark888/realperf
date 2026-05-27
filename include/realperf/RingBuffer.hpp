#pragma once

#include <bit>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace realperf {

template <typename T>
class RingBuffer {
public:
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(std::is_trivially_destructible_v<T>);

    explicit RingBuffer(std::size_t capacity)
        : capacity_(capacity)
        , byteSize_(checkedByteSize(capacity))
    {
        validateCapacity();
        fd_ = createTemporaryBackingFile(byteSize_);
        try {
            mapTwice();
        } catch (...) {
            release();
            throw;
        }
    }

    RingBuffer(std::size_t capacity, std::string_view path)
        : capacity_(capacity)
        , byteSize_(checkedByteSize(capacity))
    {
        validateCapacity();
        fd_ = createFileBacking(path, byteSize_);
        try {
            mapTwice();
        } catch (...) {
            release();
            throw;
        }
    }

    RingBuffer(std::string_view path, std::size_t capacity)
        : RingBuffer(capacity, path)
    {
    }

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    RingBuffer(RingBuffer&& other) noexcept
    {
        moveFrom(std::move(other));
    }

    RingBuffer& operator=(RingBuffer&& other) noexcept
    {
        if (this != &other) {
            release();
            moveFrom(std::move(other));
        }

        return *this;
    }

    ~RingBuffer()
    {
        release();
    }

    std::size_t capacity() const
    {
        return capacity_;
    }

    std::size_t byteSize() const
    {
        return byteSize_;
    }

    T* data()
    {
        return data_;
    }

    const T* data() const
    {
        return data_;
    }

    T& operator[](std::size_t index)
    {
        return data_[index & (capacity_ - 1u)];
    }

    const T& operator[](std::size_t index) const
    {
        return data_[index & (capacity_ - 1u)];
    }

private:
    void validateCapacity() const
    {
        if (!std::has_single_bit(capacity_)) {
            throw std::invalid_argument("RingBuffer capacity must be a power of two");
        }

        const long pageSize = ::sysconf(_SC_PAGESIZE);
        if (pageSize <= 0) {
            throwSystemError("sysconf(_SC_PAGESIZE)");
        }

        if (byteSize_ % static_cast<std::size_t>(pageSize) != 0u) {
            throw std::invalid_argument("RingBuffer byte size must be a multiple of the system page size");
        }
    }

    static std::size_t checkedByteSize(std::size_t capacity)
    {
        if (capacity == 0u) {
            throw std::invalid_argument("RingBuffer capacity must not be zero");
        }

        if (capacity > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::overflow_error("RingBuffer byte size overflows size_t");
        }

        return capacity * sizeof(T);
    }

    static void throwSystemError(const char* operation)
    {
        throw std::runtime_error(
            std::string(operation) + " failed: " + std::strerror(errno));
    }

    static int createTemporaryBackingFile(std::size_t byteSize)
    {
        char path[] = "/tmp/realperf-ring-buffer-XXXXXX";
        const int fd = ::mkstemp(path);
        if (fd == -1) {
            throwSystemError("mkstemp");
        }

        ::unlink(path);

        if (::ftruncate(fd, static_cast<off_t>(byteSize)) == -1) {
            const int savedErrno = errno;
            ::close(fd);
            errno = savedErrno;
            throwSystemError("ftruncate");
        }

        return fd;
    }

    static int createFileBacking(std::string_view path, std::size_t byteSize)
    {
        if (path.empty()) {
            throw std::invalid_argument("RingBuffer backing file path must not be empty");
        }

        const std::string pathString {path};
        const int fd = ::open(pathString.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0600);
        if (fd == -1) {
            throwSystemError("open backing file");
        }

        if (::ftruncate(fd, static_cast<off_t>(byteSize)) == -1) {
            const int savedErrno = errno;
            ::close(fd);
            errno = savedErrno;
            throwSystemError("ftruncate backing file");
        }

        return fd;
    }

    void mapTwice()
    {
        if (byteSize_ > std::numeric_limits<std::size_t>::max() / 2u) {
            throw std::overflow_error("RingBuffer mirrored byte size overflows size_t");
        }

        void* reservation = ::mmap(
            nullptr,
            byteSize_ * 2u,
            PROT_NONE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1,
            0);
        if (reservation == MAP_FAILED) {
            throwSystemError("mmap reservation");
        }

        void* first = ::mmap(
            reservation,
            byteSize_,
            PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_FIXED,
            fd_,
            0);
        if (first == MAP_FAILED) {
            const int saved_errno = errno;
            ::munmap(reservation, byteSize_ * 2u);
            errno = saved_errno;
            throwSystemError("mmap first view");
        }

        void* secondAddress = static_cast<std::byte*>(reservation) + byteSize_;
        void* second = ::mmap(
            secondAddress,
            byteSize_,
            PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_FIXED,
            fd_,
            0);
        if (second == MAP_FAILED) {
            const int savedErrno = errno;
            ::munmap(reservation, byteSize_ * 2u);
            errno = savedErrno;
            throwSystemError("mmap second view");
        }

        data_ = static_cast<T*>(reservation);
    }

    void moveFrom(RingBuffer&& other) noexcept
    {
        capacity_ = std::exchange(other.capacity_, 0u);
        byteSize_ = std::exchange(other.byteSize_, 0u);
        data_ = std::exchange(other.data_, nullptr);
        fd_ = std::exchange(other.fd_, -1);
    }

    void release() noexcept
    {
        if (data_ != nullptr) {
            ::munmap(data_, byteSize_ * 2u);
        }

        if (fd_ != -1) {
            ::close(fd_);
        }

        capacity_ = 0u;
        byteSize_ = 0u;
        data_ = nullptr;
        fd_ = -1;
    }

    std::size_t capacity_ {};
    std::size_t byteSize_ {};
    T* data_ {};
    int fd_ {-1};
};

} // namespace realperf
