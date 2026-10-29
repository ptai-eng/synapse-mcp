cmake_minimum_required(VERSION 3.20)
project(synapse_mcp VERSION 0.2.0 LANGUAGES CXX)

# The checked-in 3.11.3 headers are the one nlohmann/json source for the SDK.
# Do not combine them with an unrelated parent target's include paths.
set(_synapse_json_include "${CMAKE_CURRENT_SOURCE_DIR}/include")
foreach(_json_target IN ITEMS nlohmann_json nlohmann_json::nlohmann_json)
    if(TARGET "${_json_target}")
        get_target_property(_json_source "${_json_target}" SYNAPSE_NLOHMANN_JSON_INCLUDE_DIR)
        if(NOT _json_source STREQUAL _synapse_json_include)
            message(FATAL_ERROR "Pre-existing nlohmann/json target is incompatible with Synapse's vendored 3.11.3 source")
        endif()
    endif()
endforeach()
if(NOT TARGET nlohmann_json::nlohmann_json)
    add_library(synapse_nlohmann_json INTERFACE)
    add_library(nlohmann_json::nlohmann_json ALIAS synapse_nlohmann_json)
    target_include_directories(synapse_nlohmann_json INTERFACE
        $<BUILD_INTERFACE:${_synapse_json_include}>
        $<INSTALL_INTERFACE:include>)
    set_property(TARGET synapse_nlohmann_json PROPERTY
        SYNAPSE_NLOHMANN_JSON_INCLUDE_DIR "${_synapse_json_include}")
endif()

include(cmake/JsonconsDependency.cmake)
synapse_add_jsoncons_dependency()

# Define synapse-mcp as an interface (header-only) library
add_library(synapse_mcp INTERFACE)
add_library(synapse::mcp ALIAS synapse_mcp)
target_compile_features(synapse_mcp INTERFACE cxx_std_23)

target_include_directories(synapse_mcp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_package(Threads REQUIRED)
target_link_libraries(synapse_mcp INTERFACE
    nlohmann_json::nlohmann_json jsoncons::jsoncons Threads::Threads)

set(_synapse_top_level OFF)
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(_synapse_top_level ON)
endif()

# Examples
option(SYNAPSE_MCP_BUILD_EXAMPLES "Build Synapse MCP examples" "${_synapse_top_level}")
if(SYNAPSE_MCP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

option(SYNAPSE_MCP_BUILD_INTEROP_TESTS "Build official client interoperability tests" OFF)
# Tests
option(SYNAPSE_MCP_BUILD_TESTS "Build Synapse MCP tests" "${_synapse_top_level}")
if(SYNAPSE_MCP_BUILD_INTEROP_TESTS AND NOT SYNAPSE_MCP_BUILD_TESTS)
    message(FATAL_ERROR "Interop tests require SYNAPSE_MCP_BUILD_TESTS=ON")
endif()
if(SYNAPSE_MCP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
