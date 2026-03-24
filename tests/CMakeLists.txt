cmake_minimum_required(VERSION 3.14)
project(meow_godot_mcp_tests)

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
    ${CMAKE_SOURCE_DIR}/../src/mcp_tool_registry.cpp
    ${CMAKE_SOURCE_DIR}/../src/mcp_prompts.cpp
)
target_link_libraries(test_protocol GTest::gtest_main)

# Scene tree format tests (tests JSON structure contract, no Godot dependency)
add_executable(test_scene_tree test_scene_tree.cpp)
target_link_libraries(test_scene_tree GTest::gtest_main)

# Variant parser tests (pure C++17, no Godot dependency)
add_executable(test_variant_parser
    test_variant_parser.cpp
    ${CMAKE_SOURCE_DIR}/../src/variant_parser.cpp
)
target_link_libraries(test_variant_parser GTest::gtest_main)

# Scene mutation argument/response contract tests (no Godot dependency)
add_executable(test_scene_mutation test_scene_mutation.cpp)
target_link_libraries(test_scene_mutation GTest::gtest_main)

# Script tools unit tests (pure C++ edit_lines + validate_res_path, no Godot dependency)
add_executable(test_script_tools
    test_script_tools.cpp
    ${CMAKE_SOURCE_DIR}/../src/script_tools.cpp
)
target_link_libraries(test_script_tools GTest::gtest_main)

# Tool registry unit tests (version comparison, tool filtering, no Godot dependency)
add_executable(test_tool_registry
    test_tool_registry.cpp
    ${CMAKE_SOURCE_DIR}/../src/mcp_tool_registry.cpp
)
target_link_libraries(test_tool_registry GTest::gtest_main)

# Composite tools tests (pure C++ find_nodes_match_name, no Godot dependency)
add_executable(test_composite_tools
    test_composite_tools.cpp
    ${CMAKE_SOURCE_DIR}/../src/composite_tools.cpp
)
target_link_libraries(test_composite_tools GTest::gtest_main)

# Prompt template unit tests (validates prompt content and tool references)
add_executable(test_prompts
    test_prompts.cpp
    ${CMAKE_SOURCE_DIR}/../src/mcp_prompts.cpp
    ${CMAKE_SOURCE_DIR}/../src/mcp_tool_registry.cpp
)
target_link_libraries(test_prompts GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(test_placeholder)
gtest_discover_tests(test_protocol)
gtest_discover_tests(test_scene_tree)
gtest_discover_tests(test_variant_parser)
gtest_discover_tests(test_scene_mutation)
gtest_discover_tests(test_script_tools)
gtest_discover_tests(test_tool_registry)
gtest_discover_tests(test_composite_tools)
gtest_discover_tests(test_prompts)
