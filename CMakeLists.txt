cmake_minimum_required(VERSION 3.20)
project(synapse_mcp VERSION 0.1.0 LANGUAGES CXX)

# Set C++23 standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# Fetch nlohmann/json
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

# Define synapse-mcp as an interface (header-only) library
add_library(synapse_mcp INTERFACE)
add_library(synapse::mcp ALIAS synapse_mcp)

target_include_directories(synapse_mcp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_package(Threads REQUIRED)
target_link_libraries(synapse_mcp INTERFACE nlohmann_json::nlohmann_json Threads::Threads)

# Examples
option(SYNAPSE_MCP_BUILD_EXAMPLES "Build Synapse MCP examples" ON)
if(SYNAPSE_MCP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
option(SYNAPSE_MCP_BUILD_TESTS "Build Synapse MCP tests" ON)
if(SYNAPSE_MCP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
