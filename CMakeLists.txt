cmake_minimum_required(VERSION 3.21)

project(realperf VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
)

FetchContent_Declare(
    magic_enum
    GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
    GIT_TAG v0.9.7
)

FetchContent_MakeAvailable(Catch2 magic_enum)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

add_library(realperf_lib
    src/LiteralString.cpp
    src/perf.cpp
)

target_include_directories(realperf_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_options(realperf_lib
    PUBLIC
        $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-msse4.1>
)

target_link_libraries(realperf_lib
    PUBLIC
        magic_enum::magic_enum
)

add_executable(realperf
    src/main.cpp
)

target_link_libraries(realperf
    PRIVATE
        realperf_lib
)

add_executable(pcap_itch_gen
    src/pcap_itch_gen.cpp
)

target_include_directories(pcap_itch_gen
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(pcap_itch_gen
    PRIVATE
        realperf_lib
)

add_executable(pcap_itch_print
    src/pcap_itch_print.cpp
)

target_include_directories(pcap_itch_print
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(pcap_itch_print
    PRIVATE
        realperf_lib
)

add_executable(checkpoint_print
    src/checkpoint_print.cpp
)

target_include_directories(checkpoint_print
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(checkpoint_print
    PRIVATE
        realperf_lib
)

add_executable(checkpoint_server
    src/checkpoint_server.cpp
)

target_include_directories(checkpoint_server
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(checkpoint_server
    PRIVATE
        realperf_lib
)

include(CTest)
if(BUILD_TESTING)
    add_executable(realperf_tests
        tests/CheckPointTest.cpp
        tests/CmdArgsTest.cpp
        tests/DoubleMapBufferTest.cpp
        tests/LiteralStringExternalTestData.cpp
        tests/LiteralStringTest.cpp
        tests/RingBufferTest.cpp
        tests/TickClockTest.cpp
        tests/itch5Test.cpp
        tests/perf_tests.cpp
    )

    target_link_libraries(realperf_tests
        PRIVATE
            realperf_lib
            Catch2::Catch2WithMain
    )

    include(Catch)
    catch_discover_tests(realperf_tests)
endif()
