cmake_minimum_required(VERSION 3.14)
project(godot_mcp_meow_tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# Include third-party headers (nlohmann/json for protocol tests)
include_directories(${CMAKE_SOURCE_DIR}/../thirdparty)

# Include source directory for protocol headers
include_directories(${CMAKE_SOURCE_DIR}/../src)

# Placeholder test executable
add_executable(test_placeholder test_placeholder.cpp)
target_link_libraries(test_placeholder GTest::gtest_main)

# Protocol unit tests (pure C++17 + nlohmann/json, no Godot dependency)
add_executable(test_protocol
    test_protocol.cpp
    ${CMAKE_SOURCE_DIR}/../src/mcp_protocol.cpp
)
target_link_libraries(test_protocol GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(test_placeholder)
gtest_discover_tests(test_protocol)
