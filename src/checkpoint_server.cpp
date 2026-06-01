#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "realperf/CheckPoint.hpp"
#include "realperf/CmdArgs.hpp"

namespace {

constexpr std::size_t kDefaultDirectBlockBytes = 1u << 20;
constexpr std::uint64_t kDefaultPollMicros = 1000;

std::atomic_bool g_stop_requested {false};

void request_stop(int)
{
    g_stop_requested.store(true, std::memory_order_relaxed);
}

std::size_t page_size()
{
    const long value = ::sysconf(_SC_PAGESIZE);
    if (value <= 0) {
        throw std::runtime_error("sysconf(_SC_PAGESIZE) failed");
    }
    return static_cast<std::size_t>(value);
}

[[noreturn]] void throw_system_error(std::string_view operation, int error = errno)
{
    throw std::runtime_error(std::string(operation) + " failed: " + std::strerror(error));
}

bool is_empty(const realperf::CheckPoint& checkpoint)
{
    return checkpoint.tick_ == 0
        && checkpoint.where_.empty()
        && checkpoint.reserved_ == 0
        && static_cast<std::uint8_t>(checkpoint.type_) == 0
        && static_cast<std::uint8_t>(checkpoint.category_) == 0;
}

struct Options {
    std::vector<std::string> inputs;
    std::filesystem::path output;
    std::filesystem::path output_dir {"."};
    std::string suffix {".dat"};
    std::size_t direct_block_bytes = kDefaultDirectBlockBytes;
    std::uint64_t poll_micros = kDefaultPollMicros;
    bool once = false;
    bool quiet = false;
};

[[noreturn]] void usage(std::string_view error = {})
{
    if (!error.empty()) {
        std::cerr << "error: " << error << "\n\n";
    }

    std::cerr
        << "usage: checkpoint_server [options] CHECKPOINT_FILE...\n\n"
        << "options:\n"
        << "  -o, --output PATH       output path for a single input\n"
        << "      --output-dir DIR    output directory for multiple inputs (default: .)\n"
        << "      --suffix TEXT       suffix for output-dir files (default: .dat)\n"
        << "      --block-bytes N     Direct I/O copy block size (default: 1048576)\n"
        << "      --poll-us N         idle poll interval in microseconds (default: 1000)\n"
        << "      --once              copy currently committed records and exit\n"
        << "      --quiet             suppress progress messages\n"
        << "  -h, --help              show this help\n\n"
        << "Each input is an existing file-backed realperf checkpoint ring buffer. The\n"
        << "server maps it read-only, \n"
        << "copied tick as committed, and writes binary CheckPoint records with O_DIRECT.\n";
    std::exit(error.empty() ? 0 : 2);
}

Options parse_options(int argc, char** argv)
{
    Options options;
    realperf::CmdArgs args(argc, argv);

    static constexpr std::array<std::string_view, 11> known_options{
        "h",
        "help",
        "o",
        "output",
        "output-dir",
        "suffix",
        "block-bytes",
        "poll-us",
        "once",
        "quiet",
        "q",
    };

    for (const auto& [name, value] : args.values()) {
        if (args.is_positional(name)) {
            continue;
        }
        if (std::find(known_options.begin(), known_options.end(), name) == known_options.end()) {
            usage("unknown option " + name);
        }
    }

    if (args.has({"h", "help"})) {
        usage();
    }
    if (const auto name = args.find({"o", "output"})) {
        options.output = args.as<std::string>(*name);
    }
    if (args.has("output-dir")) {
        options.output_dir = args.as<std::string>("output-dir");
    }
    if (args.has("suffix")) {
        options.suffix = args.as<std::string>("suffix");
    }
    if (args.has("block-bytes")) {
        options.direct_block_bytes = args.as<std::size_t>("block-bytes");
    }
    if (args.has("poll-us")) {
        options.poll_micros = args.as<std::uint64_t>("poll-us");
    }
    options.once = args.has("once");
    options.quiet = args.has({"q", "quiet"});

    for (std::size_t index = 0; index < args.positional_count(); ++index) {
        options.inputs.emplace_back(args.positional(index));
    }

    if (options.inputs.empty()) {
        usage("missing checkpoint file");
    }
    if (!options.output.empty() && options.inputs.size() != 1u) {
        usage("--output can only be used with one checkpoint file");
    }
    if (options.suffix.empty() && options.output.empty()) {
        usage("--suffix must not be empty");
    }

    return options;
}

std::filesystem::path output_path_for(const Options& options, const std::string& input, std::size_t index)
{
    if (!options.output.empty()) {
        return options.output;
    }

    auto file_name = std::filesystem::path(input).filename().string();
    if (file_name.empty()) {
        file_name = "checkpoint-" + std::to_string(index);
    }
    return options.output_dir / (file_name + options.suffix);
}

std::filesystem::path unique_output_path(
    std::filesystem::path path,
    const std::vector<std::filesystem::path>& existing,
    std::size_t index)
{
    if (std::find(existing.begin(), existing.end(), path) == existing.end()) {
        return path;
    }

    const auto parent = path.parent_path();
    const auto file_name = path.filename().string();
    path = parent / (std::to_string(index) + "-" + file_name);

    while (std::find(existing.begin(), existing.end(), path) != existing.end()) {
        path = parent / (std::to_string(index) + "-" + path.filename().string());
    }

    return path;
}

class MappedCheckPointRing {
public:
    explicit MappedCheckPointRing(std::string path)
        : path_(std::move(path))
    {
        open_file();
        try {
            validate_size();
            map_twice();
        } catch (...) {
            release();
            throw;
        }
    }

    MappedCheckPointRing(const MappedCheckPointRing&) = delete;
    MappedCheckPointRing& operator=(const MappedCheckPointRing&) = delete;

    MappedCheckPointRing(MappedCheckPointRing&& other) noexcept
    {
        move_from(std::move(other));
    }

    MappedCheckPointRing& operator=(MappedCheckPointRing&& other) noexcept
    {
        if (this != &other) {
            release();
            move_from(std::move(other));
        }
        return *this;
    }

    ~MappedCheckPointRing()
    {
        release();
    }

    const std::string& path() const
    {
        return path_;
    }

    const realperf::CheckPoint* data() const
    {
        return data_;
    }

    std::size_t capacity() const
    {
        return capacity_;
    }

    std::size_t mask() const
    {
        return capacity_ - 1u;
    }

private:
    void open_file()
    {
        fd_ = ::open(path_.c_str(), O_RDONLY);
        if (fd_ == -1) {
            throw_system_error("open checkpoint ring");
        }
    }

    void validate_size()
    {
        struct stat info {};
        if (::fstat(fd_, &info) == -1) {
            throw_system_error("fstat checkpoint ring");
        }
        if (info.st_size <= 0) {
            throw std::runtime_error("checkpoint ring is empty: " + path_);
        }

        byte_size_ = static_cast<std::size_t>(info.st_size);
        if (byte_size_ % sizeof(realperf::CheckPoint) != 0u) {
            throw std::runtime_error("checkpoint ring size is not a multiple of CheckPoint size: " + path_);
        }
        if (byte_size_ % page_size() != 0u) {
            throw std::runtime_error("checkpoint ring size is not page aligned: " + path_);
        }

        capacity_ = byte_size_ / sizeof(realperf::CheckPoint);
        if (!std::has_single_bit(capacity_)) {
            throw std::runtime_error("checkpoint ring capacity is not a power of two: " + path_);
        }
        if (byte_size_ > std::numeric_limits<std::size_t>::max() / 2u) {
            throw std::overflow_error("checkpoint ring mirrored mapping size overflows size_t");
        }
    }

    void map_twice()
    {
        void* reservation = ::mmap(
            nullptr,
            byte_size_ * 2u,
            PROT_NONE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1,
            0);
        if (reservation == MAP_FAILED) {
            throw_system_error("mmap checkpoint reservation");
        }

        void* first = ::mmap(
            reservation,
            byte_size_,
            PROT_READ,
            MAP_SHARED | MAP_FIXED,
            fd_,
            0);
        if (first == MAP_FAILED) {
            const int saved_errno = errno;
            ::munmap(reservation, byte_size_ * 2u);
            throw_system_error("mmap checkpoint first view", saved_errno);
        }

        void* second_address = static_cast<std::byte*>(reservation) + byte_size_;
        void* second = ::mmap(
            second_address,
            byte_size_,
            PROT_READ,
            MAP_SHARED | MAP_FIXED,
            fd_,
            0);
        if (second == MAP_FAILED) {
            const int saved_errno = errno;
            ::munmap(reservation, byte_size_ * 2u);
            throw_system_error("mmap checkpoint second view", saved_errno);
        }

        data_ = static_cast<const realperf::CheckPoint*>(reservation);
    }

    void move_from(MappedCheckPointRing&& other) noexcept
    {
        path_ = std::move(other.path_);
        byte_size_ = std::exchange(other.byte_size_, 0u);
        capacity_ = std::exchange(other.capacity_, 0u);
        data_ = std::exchange(other.data_, nullptr);
        fd_ = std::exchange(other.fd_, -1);
    }

    void release() noexcept
    {
        if (data_ != nullptr) {
            ::munmap(const_cast<realperf::CheckPoint*>(data_), byte_size_ * 2u);
        }
        if (fd_ != -1) {
            ::close(fd_);
        }

        byte_size_ = 0;
        capacity_ = 0;
        data_ = nullptr;
        fd_ = -1;
    }

    std::string path_;
    std::size_t byte_size_ {};
    std::size_t capacity_ {};
    const realperf::CheckPoint* data_ {};
    int fd_ {-1};
};

class DirectCheckPointWriter {
public:
    DirectCheckPointWriter(std::filesystem::path path, std::size_t block_bytes)
        : path_(std::move(path))
        , block_bytes_(block_bytes)
        , alignment_(page_size())
    {
        validate_block_size();
        allocate_buffer();
        open_file();
    }

    DirectCheckPointWriter(const DirectCheckPointWriter&) = delete;
    DirectCheckPointWriter& operator=(const DirectCheckPointWriter&) = delete;

    DirectCheckPointWriter(DirectCheckPointWriter&& other) noexcept
    {
        move_from(std::move(other));
    }

    DirectCheckPointWriter& operator=(DirectCheckPointWriter&& other) noexcept
    {
        if (this != &other) {
            release();
            move_from(std::move(other));
        }
        return *this;
    }

    ~DirectCheckPointWriter()
    {
        release();
    }

    const std::filesystem::path& path() const
    {
        return path_;
    }

    void append(const realperf::CheckPoint* checkpoints, std::size_t slots)
    {
        const auto* source = reinterpret_cast<const std::byte*>(checkpoints);
        std::size_t remaining = slots * sizeof(realperf::CheckPoint);

        while (remaining > 0u) {
            const std::size_t available = block_bytes_ - buffered_bytes_;
            const std::size_t copied = std::min(available, remaining);
            std::memcpy(static_cast<std::byte*>(buffer_) + buffered_bytes_, source, copied);

            buffered_bytes_ += copied;
            source += copied;
            remaining -= copied;

            if (buffered_bytes_ == block_bytes_) {
                flush_block();
            }
        }
    }

    void flush_padded()
    {
        if (buffered_bytes_ == 0u) {
            return;
        }

        std::memset(static_cast<std::byte*>(buffer_) + buffered_bytes_, 0, block_bytes_ - buffered_bytes_);
        buffered_bytes_ = block_bytes_;
        flush_block();
    }

    std::uint64_t direct_blocks_written() const
    {
        return direct_blocks_written_;
    }

private:
    void validate_block_size() const
    {
        if (block_bytes_ == 0u) {
            throw std::invalid_argument("--block-bytes must be greater than zero");
        }
        if (block_bytes_ % alignment_ != 0u) {
            throw std::invalid_argument("--block-bytes must be a multiple of the system page size");
        }
        if (block_bytes_ % sizeof(realperf::CheckPoint) != 0u) {
            throw std::invalid_argument("--block-bytes must be a multiple of CheckPoint size");
        }
    }

    void allocate_buffer()
    {
        if (::posix_memalign(&buffer_, alignment_, block_bytes_) != 0) {
            throw std::runtime_error("posix_memalign failed");
        }
    }

    void open_file()
    {
        const int flags = O_WRONLY | O_CREAT | O_TRUNC | O_DIRECT;
        fd_ = ::open(path_.c_str(), flags, 0644);
        if (fd_ == -1) {
            throw_system_error("open Direct I/O output");
        }
    }

    void flush_block()
    {
        std::size_t written = 0;
        const auto* data = static_cast<const std::byte*>(buffer_);

        while (written < block_bytes_) {
            const ssize_t result = ::pwrite(
                fd_,
                data + written,
                block_bytes_ - written,
                static_cast<off_t>(file_offset_ + written));
            if (result == -1) {
                if (errno == EINTR) {
                    continue;
                }
                throw_system_error("pwrite Direct I/O output");
            }
            if (result == 0) {
                throw std::runtime_error("pwrite Direct I/O output wrote zero bytes");
            }
            written += static_cast<std::size_t>(result);
        }

        file_offset_ += block_bytes_;
        buffered_bytes_ = 0;
        ++direct_blocks_written_;
    }

    void move_from(DirectCheckPointWriter&& other) noexcept
    {
        path_ = std::move(other.path_);
        block_bytes_ = std::exchange(other.block_bytes_, 0u);
        alignment_ = std::exchange(other.alignment_, 0u);
        buffered_bytes_ = std::exchange(other.buffered_bytes_, 0u);
        file_offset_ = std::exchange(other.file_offset_, 0u);
        direct_blocks_written_ = std::exchange(other.direct_blocks_written_, 0u);
        buffer_ = std::exchange(other.buffer_, nullptr);
        fd_ = std::exchange(other.fd_, -1);
    }

    void release() noexcept
    {
        if (fd_ != -1) {
            ::close(fd_);
        }
        std::free(buffer_);

        buffer_ = nullptr;
        fd_ = -1;
    }

    std::filesystem::path path_;
    std::size_t block_bytes_ {};
    std::size_t alignment_ {};
    std::size_t buffered_bytes_ {};
    std::uint64_t file_offset_ {};
    std::uint64_t direct_blocks_written_ {};
    void* buffer_ {};
    int fd_ {-1};
};

class CheckPointSource {
public:
    CheckPointSource(std::string input_path, std::filesystem::path output_path, const Options& options)
        : ring_(std::move(input_path))
        , writer_(std::move(output_path), options.direct_block_bytes)
    {
    }

    const std::string& input_path() const
    {
        return ring_.path();
    }

    const std::filesystem::path& output_path() const
    {
        return writer_.path();
    }

    std::size_t capacity() const
    {
        return ring_.capacity();
    }

    bool copy_available()
    {
        bool copied_any = false;

        while (true) {
            realperf::Tick run_last_tick = last_tick_;
            std::uint64_t logical_records = 0;
            const std::size_t slots = committed_run_slots(run_last_tick, logical_records);
            if (slots == 0u) {
                break;
            }

            writer_.append(ring_.data() + next_index_, slots);
            next_index_ = (next_index_ + slots) & ring_.mask();
            last_tick_ = run_last_tick;
            copied_slots_ += slots;
            copied_logical_records_ += logical_records;
            copied_any = true;

            if (slots < ring_.capacity()) {
                break;
            }
        }

        return copied_any;
    }

    void flush()
    {
        writer_.flush_padded();
    }

    realperf::Tick last_tick() const
    {
        return last_tick_;
    }

    std::uint64_t copied_slots() const
    {
        return copied_slots_;
    }

    std::uint64_t copied_logical_records() const
    {
        return copied_logical_records_;
    }

    std::uint64_t direct_blocks_written() const
    {
        return writer_.direct_blocks_written();
    }

private:
    std::size_t checkpoint_slots(const realperf::CheckPoint& checkpoint) const
    {
        const std::size_t extra_slots =
            (static_cast<std::size_t>(checkpoint.reserved_) + sizeof(realperf::CheckPoint) - 1u)
            / sizeof(realperf::CheckPoint);
        const std::size_t slots = 1u + extra_slots;
        if (slots > ring_.capacity()) {
            throw std::runtime_error("checkpoint record extension exceeds ring capacity: " + ring_.path());
        }
        return slots;
    }

    std::size_t committed_run_slots(realperf::Tick& run_last_tick, std::uint64_t& logical_records) const
    {
        std::size_t slots = 0;

        while (slots < ring_.capacity()) {
            const realperf::CheckPoint& checkpoint = ring_.data()[next_index_ + slots];
            if (is_empty(checkpoint) || checkpoint.tick_ <= run_last_tick) {
                break;
            }

            std::atomic_thread_fence(std::memory_order_acquire);

            const std::size_t checkpoint_slot_count = checkpoint_slots(checkpoint);
            if (slots + checkpoint_slot_count > ring_.capacity()) {
                break;
            }

            slots += checkpoint_slot_count;
            run_last_tick = checkpoint.tick_;
            ++logical_records;
        }

        return slots;
    }

    MappedCheckPointRing ring_;
    DirectCheckPointWriter writer_;
    std::size_t next_index_ {};
    realperf::Tick last_tick_ {};
    std::uint64_t copied_slots_ {};
    std::uint64_t copied_logical_records_ {};
};

void prepare_output_directories(const Options& options)
{
    if (!options.output.empty()) {
        const auto parent = options.output.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent);
        }
        return;
    }

    std::filesystem::create_directories(options.output_dir);
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const Options options = parse_options(argc, argv);
        prepare_output_directories(options);

        std::signal(SIGINT, request_stop);
        std::signal(SIGTERM, request_stop);

        std::vector<CheckPointSource> sources;
        std::vector<std::filesystem::path> output_paths;
        sources.reserve(options.inputs.size());
        output_paths.reserve(options.inputs.size());
        for (std::size_t index = 0; index < options.inputs.size(); ++index) {
            auto output_path = output_path_for(options, options.inputs[index], index);
            if (options.output.empty()) {
                output_path = unique_output_path(std::move(output_path), output_paths, index);
            }
            output_paths.push_back(output_path);

            sources.emplace_back(
                options.inputs[index],
                std::move(output_path),
                options);

            if (!options.quiet) {
                std::cerr << "checkpoint_server: "
                          << sources.back().input_path() << " -> " << sources.back().output_path()
                          << " (" << sources.back().capacity() << " slots)\n";
            }
        }

        do {
            bool copied = false;
            for (auto& source : sources) {
                copied = source.copy_available() || copied;
            }

            if (options.once || g_stop_requested.load(std::memory_order_relaxed)) {
                break;
            }
            if (!copied) {
                std::this_thread::sleep_for(std::chrono::microseconds(options.poll_micros));
            }
        } while (true);

        for (auto& source : sources) {
            source.flush();
        }

        if (!options.quiet) {
            for (const auto& source : sources) {
                std::cerr << "checkpoint_server: copied "
                          << source.copied_logical_records() << " logical records ("
                          << source.copied_slots() << " slots) from " << source.input_path()
                          << ", last_tick=" << source.last_tick()
                          << ", direct_blocks=" << source.direct_blocks_written() << '\n';
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "checkpoint_server: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
