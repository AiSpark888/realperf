#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "realperf/CheckPoint.hpp"
#include "realperf/CmdArgs.hpp"
#include "realperf/RingBuffer.hpp"
#include "realperf/itch5.hpp"

namespace {

constexpr std::size_t kEthernetHeaderSize = 14;
constexpr std::size_t kIpv4HeaderSize = 20;
constexpr std::size_t kUdpHeaderSize = 8;
constexpr std::size_t kRawMinPacketSize = kEthernetHeaderSize + kIpv4HeaderSize + kUdpHeaderSize;
constexpr std::size_t kItchMinPacketSize =
    kRawMinPacketSize + realperf::itch5::kMoldUdp64HeaderSize + 2 + realperf::itch5::kAddOrderSize;
constexpr std::size_t kDefaultCheckpointCapacity = 1u << 20;

struct Options {
    std::string output = "market_data.pcap";
    std::uint32_t count = 1000;
    std::uint32_t packet_size = 128;
    std::string mode = "itch5";
    std::string symbol = "REALPERF";
    std::string src_ip = "10.1.1.1";
    std::string dst_ip = "239.1.1.1";
    std::uint16_t src_port = 50000;
    std::uint16_t dst_port = 26477;
    std::uint32_t seed = 1;
    std::string checkpoint_file;
    std::size_t checkpoint_capacity = kDefaultCheckpointCapacity;
};

[[noreturn]] void usage(std::string_view error = {})
{
    if (!error.empty()) {
        std::cerr << "error: " << error << "\n\n";
    }

    std::cerr
        << "usage: pcap_itch_gen [options]\n\n"
        << "options:\n"
        << "  -o, --output PATH       output pcap path (default: market_data.pcap)\n"
        << "  -n, --count N           number of packets (default: 1000)\n"
        << "  -s, --size BYTES        packet size at Ethernet layer (default: 128)\n"
        << "  -m, --mode raw|itch5    payload mode (default: itch5)\n"
        << "      --symbol SYMBOL     ITCH stock symbol, max 8 chars (default: REALPERF)\n"
        << "      --src-ip ADDR       source IPv4 address (default: 10.1.1.1)\n"
        << "      --dst-ip ADDR       destination IPv4 address (default: 239.1.1.1)\n"
        << "      --src-port PORT     source UDP port (default: 50000)\n"
        << "      --dst-port PORT     destination UDP port (default: 26477)\n"
        << "      --seed N            deterministic RNG seed (default: 1)\n"
        << "      --checkpoint-file PATH\n"
        << "                           file-backed checkpoint ring buffer path\n"
        << "      --checkpoint-capacity N\n"
        << "                           checkpoint record capacity (default: 1048576)\n"
        << "  -h, --help              show this help\n\n"
        << "ITCH5 mode emits UDP packets with a MoldUDP64 header followed by one\n"
        << "length-prefixed Nasdaq ITCH 5.0-style message per packet. Extra bytes are\n"
        << "zero padding so every generated packet is exactly --size bytes.\n";
    std::exit(error.empty() ? 0 : 2);
}

template <typename UInt>
UInt parse_uint(std::string_view value, std::string_view name)
{
    UInt result{};
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto parsed = std::from_chars(begin, end, result);
    if (parsed.ec != std::errc{} || parsed.ptr != end) {
        throw std::runtime_error("invalid integer for " + std::string{name});
    }
    return result;
}

Options parse_options(int argc, char** argv)
{
    Options options;
    realperf::CmdArgs args(argc, argv);

    static constexpr std::array<std::string_view, 18> known_options{
        "h",
        "help",
        "o",
        "output",
        "n",
        "count",
        "s",
        "size",
        "m",
        "mode",
        "symbol",
        "src-ip",
        "dst-ip",
        "src-port",
        "dst-port",
        "seed",
        "checkpoint-file",
        "checkpoint-capacity",
    };

    for (const auto& [name, value] : args.values()) {
        if (args.is_positional(name)) {
            usage("unknown argument " + value);
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
    if (const auto name = args.find({"n", "count"})) {
        options.count = args.as<std::uint32_t>(*name);
    }
    if (const auto name = args.find({"s", "size"})) {
        options.packet_size = args.as<std::uint32_t>(*name);
    }
    if (const auto name = args.find({"m", "mode"})) {
        options.mode = args.as<std::string>(*name);
    }
    if (args.has("symbol")) {
        options.symbol = args.as<std::string>("symbol");
    }
    if (args.has("src-ip")) {
        options.src_ip = args.as<std::string>("src-ip");
    }
    if (args.has("dst-ip")) {
        options.dst_ip = args.as<std::string>("dst-ip");
    }
    if (args.has("src-port")) {
        options.src_port = args.as<std::uint16_t>("src-port");
    }
    if (args.has("dst-port")) {
        options.dst_port = args.as<std::uint16_t>("dst-port");
    }
    if (args.has("seed")) {
        options.seed = args.as<std::uint32_t>("seed");
    }
    if (args.has("checkpoint-file")) {
        options.checkpoint_file = args.as<std::string>("checkpoint-file");
    }
    if (args.has("checkpoint-capacity")) {
        options.checkpoint_capacity = args.as<std::size_t>("checkpoint-capacity");
    }

    if (options.count == 0) {
        throw std::runtime_error("--count must be greater than zero");
    }
    if (options.mode != "raw" && options.mode != "itch5") {
        throw std::runtime_error("--mode must be raw or itch5");
    }
    if (options.mode == "raw" && options.packet_size < kRawMinPacketSize) {
        throw std::runtime_error("--size is too small for Ethernet/IPv4/UDP");
    }
    if (options.mode == "itch5" && options.packet_size < kItchMinPacketSize) {
        throw std::runtime_error("--size is too small for ITCH5 over MoldUDP64; minimum is "
                                 + std::to_string(kItchMinPacketSize));
    }
    if (options.symbol.empty() || options.symbol.size() > 8) {
        throw std::runtime_error("--symbol must be 1 to 8 characters");
    }

    return options;
}

std::optional<realperf::RingBuffer<realperf::CheckPoint>> init_checkpoint_recorder(const Options& options)
{
    if (options.checkpoint_file.empty()) {
        return std::nullopt;
    }

    std::optional<realperf::RingBuffer<realperf::CheckPoint>> buffer;
    buffer.emplace(options.checkpoint_capacity, options.checkpoint_file);
    REALPERF_RECORDER_INIT(buffer->data(), buffer->data() + buffer->capacity(), 1u);
    return buffer;
}

void put_u16_le(std::ostream& out, std::uint16_t value)
{
    const std::array bytes{
        static_cast<char>(value),
        static_cast<char>(value >> 8),
    };
    out.write(bytes.data(), bytes.size());
}

void put_u32_le(std::ostream& out, std::uint32_t value)
{
    const std::array bytes{
        static_cast<char>(value),
        static_cast<char>(value >> 8),
        static_cast<char>(value >> 16),
        static_cast<char>(value >> 24),
    };
    out.write(bytes.data(), bytes.size());
}

std::array<std::uint8_t, 4> parse_ipv4(std::string_view text)
{
    REALPERF_SCOPE("pcap_itch_gen::parse_ipv4", realperf::Category::CAT_MD);

    std::array<std::uint8_t, 4> ip{};
    std::size_t start = 0;
    for (std::size_t part = 0; part < ip.size(); ++part) {
        const auto dot = text.find('.', start);
        const auto end = dot == std::string_view::npos ? text.size() : dot;
        if (end == start) {
            throw std::runtime_error("invalid IPv4 address");
        }
        const auto value = parse_uint<std::uint16_t>(text.substr(start, end - start), "IPv4 octet");
        if (value > 255) {
            throw std::runtime_error("IPv4 octet out of range");
        }
        ip[part] = static_cast<std::uint8_t>(value);
        start = end + 1;
        if ((part < ip.size() - 1) != (dot != std::string_view::npos)) {
            throw std::runtime_error("invalid IPv4 address");
        }
    }
    return ip;
}

std::uint16_t internet_checksum(const std::uint8_t* data, std::size_t size)
{
    REALPERF_SCOPE("pcap_itch_gen::internet_checksum", realperf::Category::CAT_MD);

    std::uint32_t sum = 0;
    for (std::size_t i = 0; i + 1 < size; i += 2) {
        sum += static_cast<std::uint16_t>((data[i] << 8) | data[i + 1]);
    }
    if (size % 2 != 0) {
        sum += static_cast<std::uint16_t>(data[size - 1] << 8);
    }
    while (sum >> 16) {
        sum = (sum & 0xffffU) + (sum >> 16);
    }
    return static_cast<std::uint16_t>(~sum);
}

std::vector<std::uint8_t> make_itch_message(const Options& options,
                                            std::uint32_t index,
                                            std::mt19937& rng)
{
    REALPERF_SCOPE("pcap_itch_gen::make_itch_message", realperf::Category::CAT_MD);

    const auto locate = static_cast<std::uint16_t>(1 + (index % 1024));
    const auto tracking = static_cast<std::uint16_t>(index % 65536);
    const auto timestamp = 9'500'000'000ULL + static_cast<std::uint64_t>(index) * 1'000;
    const auto order_ref = 1'000'000ULL + index;

    if (index == 0) {
        return realperf::itch5::make_system_event(locate, tracking, timestamp, 'O');
    }

    const auto roll = rng() % 10;
    if (roll < 7) {
        const char side = (rng() % 2) == 0 ? 'B' : 'S';
        const auto shares = static_cast<std::uint32_t>(100 + (rng() % 50) * 100);
        const auto price = static_cast<std::uint32_t>(10'0000 + (rng() % 5'0000));
        return realperf::itch5::make_add_order(locate, tracking, timestamp, order_ref, side, shares, options.symbol, price);
    }
    if (roll < 9) {
        const auto executed_ref = 1'000'000ULL + (rng() % index);
        const auto shares = static_cast<std::uint32_t>(100 + (rng() % 20) * 100);
        return realperf::itch5::make_order_executed(locate, tracking, timestamp, executed_ref, shares, 9'000'000ULL + index);
    }

    const auto deleted_ref = 1'000'000ULL + (rng() % index);
    return realperf::itch5::make_order_delete(locate, tracking, timestamp, deleted_ref);
}

std::vector<std::uint8_t> make_payload(const Options& options,
                                       std::uint32_t index,
                                       std::mt19937& rng,
                                       std::size_t payload_size)
{
    REALPERF_SCOPE("pcap_itch_gen::make_payload", realperf::Category::CAT_MD);

    std::vector<std::uint8_t> payload(payload_size, 0);
    if (options.mode == "raw") {
        for (std::size_t i = 0; i < payload.size(); ++i) {
            payload[i] = static_cast<std::uint8_t>((index + i) & 0xff);
        }
        return payload;
    }

    std::vector<std::uint8_t> encoded;
    encoded.reserve(payload_size);
    const std::string session = "REALPERF01";
    encoded.insert(encoded.end(), session.begin(), session.end());
    realperf::itch5::put_u64_be(encoded, static_cast<std::uint64_t>(index) + 1);
    realperf::itch5::put_u16_be(encoded, 1);

    auto message = make_itch_message(options, index, rng);
    realperf::itch5::put_u16_be(encoded, static_cast<std::uint16_t>(message.size()));
    encoded.insert(encoded.end(), message.begin(), message.end());

    if (encoded.size() > payload.size()) {
        throw std::runtime_error("packet size too small for generated ITCH message");
    }
    std::copy(encoded.begin(), encoded.end(), payload.begin());
    return payload;
}

std::vector<std::uint8_t> make_packet(const Options& options,
                                      const std::array<std::uint8_t, 4>& src_ip,
                                      const std::array<std::uint8_t, 4>& dst_ip,
                                      std::uint32_t index,
                                      std::mt19937& rng)
{
    REALPERF_SCOPE("pcap_itch_gen::make_packet", realperf::Category::CAT_MD);

    const auto payload_size = options.packet_size - kRawMinPacketSize;
    auto payload = make_payload(options, index, rng, payload_size);

    std::vector<std::uint8_t> packet;
    packet.reserve(options.packet_size);

    const std::array<std::uint8_t, 6> dst_mac{
        0x01,
        0x00,
        0x5e,
        static_cast<std::uint8_t>(dst_ip[1] & 0x7f),
        dst_ip[2],
        dst_ip[3],
    };
    const std::array<std::uint8_t, 6> src_mac{0x02, 0x00, 0x00, src_ip[1], src_ip[2], src_ip[3]};
    packet.insert(packet.end(), dst_mac.begin(), dst_mac.end());
    packet.insert(packet.end(), src_mac.begin(), src_mac.end());
    REALPERF_CHECKPOINT("pcap_itch_gen::make_packet:ip_header");
    realperf::itch5::put_u16_be(packet, 0x0800);

    const auto ipv4_start = packet.size();
    packet.push_back(0x45);
    packet.push_back(0);
    realperf::itch5::put_u16_be(packet, static_cast<std::uint16_t>(kIpv4HeaderSize + kUdpHeaderSize + payload.size()));
    realperf::itch5::put_u16_be(packet, static_cast<std::uint16_t>(index & 0xffff));
    realperf::itch5::put_u16_be(packet, 0x4000);
    packet.push_back(64);
    packet.push_back(17);
    realperf::itch5::put_u16_be(packet, 0);
    packet.insert(packet.end(), src_ip.begin(), src_ip.end());
    packet.insert(packet.end(), dst_ip.begin(), dst_ip.end());

    REALPERF_CHECKPOINT("pcap_itch_gen::make_packet:checksum");

    const auto checksum = internet_checksum(packet.data() + ipv4_start, kIpv4HeaderSize);
    packet[ipv4_start + 10] = static_cast<std::uint8_t>(checksum >> 8);
    packet[ipv4_start + 11] = static_cast<std::uint8_t>(checksum);

    realperf::itch5::put_u16_be(packet, options.src_port);
    realperf::itch5::put_u16_be(packet, options.dst_port);
    realperf::itch5::put_u16_be(packet, static_cast<std::uint16_t>(kUdpHeaderSize + payload.size()));
    realperf::itch5::put_u16_be(packet, 0);

    REALPERF_CHECKPOINT("pcap_itch_gen::make_packet:copy_payload");
    packet.insert(packet.end(), payload.begin(), payload.end());

    return packet;
}

void write_pcap_global_header(std::ostream& out)
{
    REALPERF_SCOPE("pcap_itch_gen::write_pcap_global_header", realperf::Category::CAT_MD);

    put_u32_le(out, 0xa1b2c3d4);
    put_u16_le(out, 2);
    put_u16_le(out, 4);
    put_u32_le(out, 0);
    put_u32_le(out, 0);
    put_u32_le(out, 65535);
    put_u32_le(out, 1);
}

void write_pcap_packet(std::ostream& out,
                       const std::vector<std::uint8_t>& packet,
                       std::uint64_t timestamp_us)
{
    REALPERF_SCOPE("pcap_itch_gen::write_pcap_packet", realperf::Category::CAT_MD);

    put_u32_le(out, static_cast<std::uint32_t>(timestamp_us / 1'000'000));
    put_u32_le(out, static_cast<std::uint32_t>(timestamp_us % 1'000'000));
    put_u32_le(out, static_cast<std::uint32_t>(packet.size()));
    put_u32_le(out, static_cast<std::uint32_t>(packet.size()));
    out.write(reinterpret_cast<const char*>(packet.data()), static_cast<std::streamsize>(packet.size()));
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const auto options = parse_options(argc, argv);
        [[maybe_unused]] auto checkpoint_buffer = init_checkpoint_recorder(options);
        const auto src_ip = parse_ipv4(options.src_ip);
        const auto dst_ip = parse_ipv4(options.dst_ip);

        std::ofstream out(options.output, std::ios::binary);
        if (!out) {
            throw std::runtime_error("failed to open output file: " + options.output);
        }

        std::mt19937 rng(options.seed);
        write_pcap_global_header(out);

        const auto base = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        for (std::uint32_t i = 0; i < options.count; ++i) {
            REALPERF_START("pcap_itch_gen::packet_loop", realperf::Category::CAT_MD);
            {
                REALPERF_SCOPE("pcap_itch_gen::packet_loop", realperf::Category::CAT_MD);
                const auto packet = make_packet(options, src_ip, dst_ip, i, rng);
                write_pcap_packet(out, packet, static_cast<std::uint64_t>(base + i));
            }
        }

        std::cout << "wrote " << options.count << " packets x " << options.packet_size
                  << " bytes to " << options.output << " (" << options.mode << ")\n";
    } catch (const std::exception& ex) {
        std::cerr << "pcap_itch_gen: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
