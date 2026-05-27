# realperf

A CMake-based C++20 project scaffold using Catch2 for tests and magic_enum as
an external dependency.

## Build

```sh
cmake --preset debug
cmake --build --preset debug

cmake --preset release
cmake --build --preset release
```

The presets write generated build files under `build/debug/` and
`build/release/`.

## Test

```sh
ctest --preset debug
ctest --preset release
```
realistic perf tool

## PCAP / ITCH5 Generator

These 2 apps are just used as demo how to use the macros to measure latency in code
they are all AI generated, and not optimized, as they are not the purpose of this project

Build the `pcap_itch_gen` utility with the normal CMake presets:

```sh
cmake --preset debug
cmake --build --preset debug
```

Generate fixed-size Ethernet/IPv4/UDP packets carrying MoldUDP64-wrapped
Nasdaq ITCH 5.0-style messages:

```sh
./build/debug/pcap_itch_gen --output itch.pcap --count 100000 --size 128 --mode itch5 --symbol REALPERF
```

Or generate raw UDP payload packets with deterministic bytes:

```sh
./build/debug/pcap_itch_gen -o raw.pcap -n 100000 -s 96 -m raw
```

`--size` is the captured Ethernet-frame size in bytes, excluding FCS. ITCH5
mode uses a 10-byte MoldUDP64 session, 8-byte sequence number, 2-byte message
count, then one 2-byte length-prefixed ITCH message per packet. Padding bytes
are zero-filled so every packet is exactly the requested size.

Print generated ITCH5 captures:

```sh
./build/debug/pcap_itch_print --limit 10 itch.pcap
```

Both PCAP tools can write `CheckPoint` timing records to a named
file-backed ring buffer:

```sh
./build/debug/pcap_itch_gen --checkpoint-file gen.checkpoints --output itch.pcap
./build/debug/pcap_itch_print --checkpoint-file print.checkpoints --limit 10 itch.pcap
```

Use `--checkpoint-capacity N` to override the default capacity of 1,048,576
records. The value must be a power of two and the mapped byte size must be a
multiple of the system page size.
