if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.20)
    project(SynapseSchemaQualification LANGUAGES CXX)
    enable_testing()
    option(SYNAPSE_SCHEMA_APPLY_PATCH "Apply the qualified correction (OFF is RED qualification only)" ON)

    if(SYNAPSE_SCHEMA_APPLY_PATCH)
        include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/JsonconsDependency.cmake")
        synapse_add_jsoncons_dependency()
    else()
        # This bypass is confined to this standalone RED project. SDK builds
        # always enter JsonconsDependency.cmake, which has no unpatched option.
        include(FetchContent)
        set(JSONCONS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(synapse_jsoncons_red
            DOWNLOAD_COMMAND "${CMAKE_COMMAND}" "-DJSONCONS_SOURCE_DIR=<SOURCE_DIR>"
                -DJSONCONS_FETCH_ONLY=ON
                -P "${CMAKE_CURRENT_LIST_DIR}/../../cmake/ApplyJsonconsPatch.cmake")
        FetchContent_MakeAvailable(synapse_jsoncons_red)
        add_library(jsoncons::jsoncons ALIAS jsoncons)
        target_compile_options(jsoncons INTERFACE
            "$<$<AND:$<PLATFORM_ID:Windows>,$<CXX_COMPILER_ID:GNU>>:-Wa,-mbig-obj>")
    endif()
endif()

add_executable(synapse_schema_qualification qualification.cpp)
target_link_libraries(synapse_schema_qualification PRIVATE jsoncons::jsoncons)
target_compile_features(synapse_schema_qualification PRIVATE cxx_std_17)
foreach(_case IN ITEMS pattern_nul pattern_properties_nul additional_properties_false_nul
        additional_properties_schema_nul additional_properties_walk_nul
        properties_validate_nul properties_walk_nul
        unevaluated_properties_false_nul unevaluated_properties_schema_nul)
    add_test(NAME synapse.schema.${_case} COMMAND synapse_schema_qualification ${_case})
    set_tests_properties(synapse.schema.${_case} PROPERTIES TIMEOUT 30)
endforeach()

# Building this target must fail at the explicit regex guard. A passing
# compiler invocation is a qualification failure, not a skipped test.
add_library(synapse_schema_regex_disabled OBJECT EXCLUDE_FROM_ALL qualification.cpp)
target_link_libraries(synapse_schema_regex_disabled PRIVATE jsoncons::jsoncons)
target_compile_features(synapse_schema_regex_disabled PRIVATE cxx_std_17)
target_compile_definitions(synapse_schema_regex_disabled PRIVATE SYNAPSE_QUALIFICATION_DISABLE_REGEX=1)
set(_regex_test [=[
execute_process(COMMAND "@CMAKE_COMMAND@" --build "@CMAKE_BINARY_DIR@"
    --target synapse_schema_regex_disabled --config "$<CONFIG>"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(result EQUAL 0 OR NOT "${output}${error}" MATCHES "Synapse schema validation requires JSONCONS_HAS_STD_REGEX")
    message(FATAL_ERROR "Regex-disabled compile did not fail at the required guard: ${output}${error}")
endif()
message(STATUS "Regex-disabled compile rejected by the required guard")
]=])
string(CONFIGURE "${_regex_test}" _regex_test @ONLY)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/regex-disabled-$<CONFIG>.cmake" CONTENT "${_regex_test}")
add_test(NAME synapse.schema.regex_disabled
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/regex-disabled-$<CONFIG>.cmake")
set_tests_properties(synapse.schema.regex_disabled PROPERTIES TIMEOUT 60)

get_target_property(QUALIFIED_JSONCONS_SOURCE_DIR jsoncons SYNAPSE_JSONCONS_SOURCE_DIR)
if(QUALIFIED_JSONCONS_SOURCE_DIR)
    get_filename_component(QUALIFICATION_PATCH_SCRIPT
        "${CMAKE_CURRENT_LIST_DIR}/../../cmake/ApplyJsonconsPatch.cmake" ABSOLUTE)
    set(_correction_test [=[
set(source "@QUALIFIED_JSONCONS_SOURCE_DIR@")
set(script "@QUALIFICATION_PATCH_SCRIPT@")
set(relative "include/jsoncons_ext/jsonschema/common/keyword_validator.hpp")
# Whole-file postimage fixture, independently frozen when producing the patch.
set(expected "64e377f5b8434231887cfb25cb68cf58e657336c5a65998bc971188c66643fa2")
file(SHA256 "${source}/${relative}" before)
if(NOT before STREQUAL expected)
    message(FATAL_ERROR "Qualification postimage digest differs from its regression fixture")
endif()
if(TEST_MISMATCH)
    set(source "@CMAKE_CURRENT_BINARY_DIR@/mismatched-jsoncons")
    file(MAKE_DIRECTORY "${source}/include/jsoncons_ext/jsonschema/common")
    file(READ "@QUALIFIED_JSONCONS_SOURCE_DIR@/${relative}" text)
    file(CONFIGURE OUTPUT "${source}/${relative}"
        CONTENT "${text}\n// deliberate qualification mismatch\n" @ONLY NEWLINE_STYLE UNIX)
    file(SHA256 "${source}/${relative}" before)
    execute_process(COMMAND "@CMAKE_COMMAND@" "-DJSONCONS_SOURCE_DIR=${source}" -P "${script}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(result EQUAL 0 OR NOT "${output}${error}" MATCHES "preimage/postimage mismatch")
        message(FATAL_ERROR "Mismatched source was not rejected for provenance: ${output}${error}")
    endif()
else()
    foreach(attempt RANGE 1 2)
        execute_process(COMMAND "@CMAKE_COMMAND@" "-DJSONCONS_SOURCE_DIR=${source}" -P "${script}"
            RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
        if(NOT result EQUAL 0 OR NOT output MATCHES "exact postimage verified; no changes")
            message(FATAL_ERROR "Exact-postimage reapplication failed: ${output}${error}")
        endif()
    endforeach()
endif()
file(SHA256 "${source}/${relative}" after)
if(NOT before STREQUAL after)
    message(FATAL_ERROR "Correction qualification unexpectedly changed its source")
endif()
]=])
    string(CONFIGURE "${_correction_test}" _correction_test @ONLY)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/qualify-correction.cmake" "${_correction_test}")
    add_test(NAME synapse.schema.patch_idempotent
        COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/qualify-correction.cmake")
    add_test(NAME synapse.schema.preimage_mismatch
        COMMAND "${CMAKE_COMMAND}" -DTEST_MISMATCH=ON -P "${CMAKE_CURRENT_BINARY_DIR}/qualify-correction.cmake")
    set_tests_properties(synapse.schema.patch_idempotent synapse.schema.preimage_mismatch
        PROPERTIES TIMEOUT 30)
endif()
