cmake_minimum_required(VERSION 3.20)
project(synapse_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED SYNAPSE_SOURCE_DIR)
    get_filename_component(SYNAPSE_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()
get_filename_component(SYNAPSE_SOURCE_DIR "${SYNAPSE_SOURCE_DIR}" ABSOLUTE)
if(NOT EXISTS "${SYNAPSE_SOURCE_DIR}/include/synapse/protocol.hpp")
    message(FATAL_ERROR "SYNAPSE_SOURCE_DIR must name the SDK source root")
endif()

if(SYNAPSE_CONSUMER_PARENT_DEPENDENCY STREQUAL "nlohmann")
    add_library(parent_json INTERFACE)
    add_library(nlohmann_json::nlohmann_json ALIAS parent_json)
elseif(SYNAPSE_CONSUMER_PARENT_DEPENDENCY STREQUAL "jsoncons")
    add_library(jsoncons INTERFACE)
elseif(DEFINED SYNAPSE_CONSUMER_PARENT_DEPENDENCY
       AND NOT SYNAPSE_CONSUMER_PARENT_DEPENDENCY STREQUAL "")
    message(FATAL_ERROR "Unknown parent dependency fixture")
endif()

add_subdirectory("${SYNAPSE_SOURCE_DIR}" synapse-sdk)

if(NOT TARGET synapse::mcp)
    message(FATAL_ERROR "The SDK must provide synapse::mcp")
endif()
get_target_property(synapse_library_type synapse_mcp TYPE)
if(NOT synapse_library_type STREQUAL "INTERFACE_LIBRARY")
    message(FATAL_ERROR "synapse_mcp must remain an INTERFACE library")
endif()
if(NOT CMAKE_CXX_STANDARD EQUAL 17)
    message(FATAL_ERROR "Adding the SDK changed the parent C++17 default")
endif()

# A header-only dependency must not add compiled examples, tests or test frameworks
# to its embedding application's default build.
function(assert_header_only_subdirectory directory)
    get_property(directory_targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(dependency_target IN LISTS directory_targets)
        get_target_property(dependency_type "${dependency_target}" TYPE)
        if(NOT dependency_type STREQUAL "INTERFACE_LIBRARY")
            message(FATAL_ERROR
                "SDK dependency build leaked target '${dependency_target}' (${dependency_type}); examples/tests must default OFF")
        endif()
    endforeach()
    get_property(child_directories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(child_directory IN LISTS child_directories)
        assert_header_only_subdirectory("${child_directory}")
    endforeach()
endfunction()
assert_header_only_subdirectory("${SYNAPSE_SOURCE_DIR}")

add_executable(synapse_consumer main.cpp)
target_link_libraries(synapse_consumer PRIVATE synapse::mcp)
