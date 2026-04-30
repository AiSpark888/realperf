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
