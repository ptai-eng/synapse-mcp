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
foreach(_case IN ITEMS unicode_format unicode_codepoints dynamic_fragment pattern_nul pattern_properties_nul additional_properties_false_nul
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
    get_target_property(QUALIFIED_SRELL_SOURCE_DIR synapse_srell SYNAPSE_SRELL_SOURCE_DIR)
    foreach(mode IN ITEMS idempotent mismatch)
        if(mode STREQUAL "idempotent")
            set(name patch_idempotent)
        else()
            set(name preimage_mismatch)
        endif()
        add_test(NAME synapse.schema.${name} COMMAND "${CMAKE_COMMAND}"
            "-DJSONCONS_SOURCE_DIR=${QUALIFIED_JSONCONS_SOURCE_DIR}"
            "-DSRELL_SOURCE_DIR=${QUALIFIED_SRELL_SOURCE_DIR}"
            "-DTEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}" "-DTEST_MODE=${mode}"
            -P "${CMAKE_CURRENT_LIST_DIR}/../../cmake/TestDependencyProvenance.cmake")
        set_tests_properties(synapse.schema.${name} PROPERTIES TIMEOUT 120)
    endforeach()
    foreach(feature IN ITEMS DATA PROPERTY ICASE)
        set(target synapse_schema_unicode_disabled_${feature})
        add_library(${target} OBJECT EXCLUDE_FROM_ALL qualification.cpp)
        target_link_libraries(${target} PRIVATE jsoncons::jsoncons)
        target_compile_features(${target} PRIVATE cxx_std_17)
        target_compile_definitions(${target} PRIVATE SRELL_NO_UNICODE_${feature}=1)
        set(_unicode_test [=[
execute_process(COMMAND "@CMAKE_COMMAND@" --build "@CMAKE_BINARY_DIR@"
    --target @target@ --config "$<CONFIG>"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(result EQUAL 0 OR NOT "${output}${error}" MATCHES "Synapse schema validation requires SRELL Unicode support")
    message(FATAL_ERROR "Disabled Unicode compile did not fail at guard: ${output}${error}")
endif()
]=])
        string(CONFIGURE "${_unicode_test}" _unicode_test @ONLY)
        file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/unicode-${feature}-$<CONFIG>.cmake" CONTENT "${_unicode_test}")
        add_test(NAME synapse.schema.unicode_disabled_${feature} COMMAND "${CMAKE_COMMAND}"
            -P "${CMAKE_CURRENT_BINARY_DIR}/unicode-${feature}-$<CONFIG>.cmake")
        set_tests_properties(synapse.schema.unicode_disabled_${feature} PROPERTIES TIMEOUT 60)
    endforeach()
endif()

if(QUALIFIED_JSONCONS_SOURCE_DIR)
    add_test(NAME synapse.schema.reject_parent_sources COMMAND "${CMAKE_COMMAND}"
        "-DTEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}" "-DTEST_GENERATOR=${CMAKE_GENERATOR}"
        -P "${CMAKE_CURRENT_LIST_DIR}/../../cmake/TestParentDependencies.cmake")
    set_tests_properties(synapse.schema.reject_parent_sources PROPERTIES TIMEOUT 120)
endif()
