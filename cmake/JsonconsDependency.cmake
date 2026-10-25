include_guard(GLOBAL)
include(FetchContent)

function(synapse_add_jsoncons_dependency)
    # A parent's dependency may have identical version text and different
    # behavior. Only this module's owned, qualified target may be reused.
    if(TARGET jsoncons OR TARGET jsoncons::jsoncons)
        if(TARGET jsoncons)
            get_target_property(_qualified jsoncons SYNAPSE_JSONCONS_QUALIFIED)
            get_target_property(_commit jsoncons SYNAPSE_JSONCONS_COMMIT)
        endif()
        if(NOT _qualified OR NOT _commit STREQUAL "9e57e421285422d7aeeb5aa7dc839f6a19121ee6")
            message(FATAL_ERROR "Pre-existing jsoncons target has unqualified provenance/correction")
        endif()
        return()
    endif()
    if(FETCHCONTENT_SOURCE_DIR_SYNAPSE_JSONCONS)
        message(FATAL_ERROR "Synapse requires its owned build-tree jsoncons checkout; source overrides are unqualified")
    endif()
    FetchContent_GetProperties(synapse_jsoncons)
    if(synapse_jsoncons_POPULATED)
        message(FATAL_ERROR "Pre-populated jsoncons source has unqualified provenance/correction")
    endif()

    find_package(Git REQUIRED QUIET)
    set(JSONCONS_BUILD_TESTS OFF CACHE BOOL "Build upstream jsoncons tests" FORCE)
    FetchContent_Declare(synapse_jsoncons
        SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/synapse_jsoncons-src"
        BINARY_DIR "${CMAKE_BINARY_DIR}/_deps/synapse_jsoncons-build"
        DOWNLOAD_COMMAND "${CMAKE_COMMAND}" "-DJSONCONS_SOURCE_DIR=<SOURCE_DIR>"
            -DJSONCONS_FETCH_ONLY=ON
            -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ApplyJsonconsPatch.cmake"
        PATCH_COMMAND "${CMAKE_COMMAND}" "-DJSONCONS_SOURCE_DIR=<SOURCE_DIR>"
            -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ApplyJsonconsPatch.cmake")
    FetchContent_MakeAvailable(synapse_jsoncons)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${synapse_jsoncons_SOURCE_DIR}" rev-parse HEAD
        RESULT_VARIABLE _revision_result OUTPUT_VARIABLE _revision
        ERROR_VARIABLE _revision_error OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT _revision_result EQUAL 0 OR NOT _revision STREQUAL "9e57e421285422d7aeeb5aa7dc839f6a19121ee6")
        message(FATAL_ERROR "jsoncons checkout commit mismatch: ${_revision} ${_revision_error}")
    endif()
    # FetchContent skips PATCH_COMMAND on an unchanged second configure. Verify
    # again so changed files cannot be accepted through a cached stamp.
    execute_process(COMMAND "${CMAKE_COMMAND}"
        "-DJSONCONS_SOURCE_DIR=${synapse_jsoncons_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ApplyJsonconsPatch.cmake"
        RESULT_VARIABLE _qualification_result)
    if(NOT _qualification_result EQUAL 0)
        message(FATAL_ERROR "jsoncons dependency correction qualification failed")
    endif()
    add_library(jsoncons::jsoncons ALIAS jsoncons)
    # Schema template instantiations exceed COFF's ordinary section limit on
    # the supported MinGW toolchain, including in downstream consumers.
    target_compile_options(jsoncons INTERFACE
        "$<$<AND:$<PLATFORM_ID:Windows>,$<CXX_COMPILER_ID:GNU>>:-Wa,-mbig-obj>")
    set_property(TARGET jsoncons PROPERTY SYNAPSE_JSONCONS_QUALIFIED TRUE)
    set_property(TARGET jsoncons PROPERTY SYNAPSE_JSONCONS_COMMIT "${_revision}")
    set_property(TARGET jsoncons PROPERTY SYNAPSE_JSONCONS_SOURCE_DIR "${synapse_jsoncons_SOURCE_DIR}")
endfunction()
