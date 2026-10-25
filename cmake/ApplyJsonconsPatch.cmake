# jsoncons v1.10.0 / 9e57e421285422d7aeeb5aa7dc839f6a19121ee6.
# BSL-1.0: retain the upstream LICENSE and copyright notices unchanged.
# ADR-003: remove this correction only after qualifying an upstream fix for
# pattern searches and property lookups past embedded NULs. Only downloaded
# build-tree files change; diagnostics and validator branching stay unchanged.
cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED JSONCONS_SOURCE_DIR)
    message(FATAL_ERROR "JSONCONS_SOURCE_DIR must name the downloaded jsoncons source")
endif()
get_filename_component(JSONCONS_SOURCE_DIR "${JSONCONS_SOURCE_DIR}" ABSOLUTE)
find_package(Git REQUIRED QUIET)

if(JSONCONS_FETCH_ONLY)
    # Fetch exactly the frozen commit without downloading upstream history.
    file(MAKE_DIRECTORY "${JSONCONS_SOURCE_DIR}")
    foreach(_step IN ITEMS init lf fetch checkout)
        if(_step STREQUAL "init")
            set(_arguments init)
        elseif(_step STREQUAL "lf")
            set(_arguments config core.autocrlf false)
        elseif(_step STREQUAL "fetch")
            set(_arguments fetch --depth=1 https://github.com/danielaparker/jsoncons.git
                9e57e421285422d7aeeb5aa7dc839f6a19121ee6)
        else()
            set(_arguments checkout --detach FETCH_HEAD)
        endif()
        execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${JSONCONS_SOURCE_DIR}" ${_arguments}
            RESULT_VARIABLE _download_result)
        if(NOT _download_result EQUAL 0)
            message(FATAL_ERROR "jsoncons pinned download failed during ${_step}")
        endif()
    endforeach()
    return()
endif()

set(_validator "${JSONCONS_SOURCE_DIR}/include/jsoncons_ext/jsonschema/common/keyword_validator.hpp")
set(_preimage_blob "45e9040c2b65e44bb102c712ea91f49fe2b0a2b0")
set(_postimage_sha256 "64e377f5b8434231887cfb25cb68cf58e657336c5a65998bc971188c66643fa2")
if(NOT EXISTS "${_validator}")
    message(FATAL_ERROR "jsoncons preimage/postimage mismatch: validator is missing")
endif()
file(READ "${_validator}" _contents)
string(FIND "${_contents}" "\r" _carriage_return)
if(NOT _carriage_return EQUAL -1)
    message(FATAL_ERROR "jsoncons validator must use LF checkout (core.autocrlf=false)")
endif()
file(SHA256 "${_validator}" _actual_sha256)
if(_actual_sha256 STREQUAL _postimage_sha256)
    message(STATUS "jsoncons correction: exact postimage verified; no changes")
    return()
endif()

execute_process(COMMAND "${GIT_EXECUTABLE}" hash-object --no-filters "${_validator}"
    RESULT_VARIABLE _hash_result OUTPUT_VARIABLE _actual_blob
    ERROR_VARIABLE _hash_error OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _hash_result EQUAL 0 OR NOT _actual_blob STREQUAL _preimage_blob)
    message(FATAL_ERROR "jsoncons preimage/postimage mismatch: expected blob ${_preimage_blob}, got '${_actual_blob}' ${_hash_error}")
endif()

set(_preimage "            if (!std::regex_search(instance.as_cstring(), regex_))\n")
string(REPLACE "${_preimage}" "" _without_preimage "${_contents}")
string(LENGTH "${_contents}" _original_size)
string(LENGTH "${_without_preimage}" _without_size)
string(LENGTH "${_preimage}" _preimage_size)
math(EXPR _removed_size "${_original_size} - ${_without_size}")
if(NOT _removed_size EQUAL _preimage_size)
    message(FATAL_ERROR "jsoncons correction requires exactly one expected preimage")
endif()
foreach(_lookup IN ITEMS properties_ allowed_properties results.evaluated_properties)
    if(_lookup STREQUAL "allowed_properties")
        set(_expected_count 3)
    else()
        set(_expected_count 2)
    endif()
    set(_needle "${_lookup}.find(prop.key().c_str())")
    string(REPLACE "${_needle}" "" _without_lookup "${_contents}")
    string(LENGTH "${_without_lookup}" _without_size)
    string(LENGTH "${_needle}" _needle_size)
    math(EXPR _expected_removed "${_expected_count} * ${_needle_size}")
    math(EXPR _removed_size "${_original_size} - ${_without_size}")
    if(NOT _removed_size EQUAL _expected_removed)
        message(FATAL_ERROR "jsoncons correction requires ${_expected_count} ${_lookup} lookup preimages")
    endif()
endforeach()

# Normalize this project's patch artifact too: the dependency is always LF,
# even when Synapse itself was checked out with core.autocrlf=true on Windows.
file(READ "${CMAKE_CURRENT_LIST_DIR}/patches/jsoncons-v1.10.0-pattern-range.patch" _patch)
string(REPLACE "\r\n" "\n" _patch "${_patch}")
set(_build_patch "${JSONCONS_SOURCE_DIR}/../synapse-jsoncons-pattern-range.patch")
file(CONFIGURE OUTPUT "${_build_patch}" CONTENT "${_patch}" @ONLY NEWLINE_STYLE UNIX)
execute_process(COMMAND "${GIT_EXECUTABLE}" -c core.autocrlf=false apply --check --unidiff-zero "${_build_patch}"
    WORKING_DIRECTORY "${JSONCONS_SOURCE_DIR}" RESULT_VARIABLE _check_result
    OUTPUT_VARIABLE _check_output ERROR_VARIABLE _check_error)
if(NOT _check_result EQUAL 0)
    message(FATAL_ERROR "jsoncons correction check failed: ${_check_output}${_check_error}")
endif()
execute_process(COMMAND "${GIT_EXECUTABLE}" -c core.autocrlf=false apply --unidiff-zero "${_build_patch}"
    WORKING_DIRECTORY "${JSONCONS_SOURCE_DIR}" RESULT_VARIABLE _apply_result
    OUTPUT_VARIABLE _apply_output ERROR_VARIABLE _apply_error)
if(NOT _apply_result EQUAL 0)
    message(FATAL_ERROR "jsoncons correction failed: ${_apply_output}${_apply_error}")
endif()
file(SHA256 "${_validator}" _actual_sha256)
if(NOT _actual_sha256 STREQUAL _postimage_sha256)
    message(FATAL_ERROR "jsoncons correction postimage digest mismatch: ${_actual_sha256}")
endif()
message(STATUS "jsoncons correction: pinned preimage and exact postimage verified")
