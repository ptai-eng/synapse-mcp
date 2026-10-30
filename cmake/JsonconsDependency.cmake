include_guard(GLOBAL)
include(FetchContent)
include("${CMAKE_CURRENT_LIST_DIR}/SrellDependency.cmake")
function(synapse_add_jsoncons_dependency)
    if(TARGET jsoncons OR TARGET jsoncons::jsoncons)
        get_property(owned GLOBAL PROPERTY SYNAPSE_OWNED_JSONCONS)
        if(NOT owned OR NOT TARGET jsoncons)
            message(FATAL_ERROR "Pre-existing jsoncons target has unqualified provenance/correction")
        endif()
        get_target_property(source jsoncons SYNAPSE_JSONCONS_SOURCE_DIR)
    else()
        get_property(declared GLOBAL PROPERTY _FetchContent_synapse_jsoncons_savedDetails SET)
        FetchContent_GetProperties(synapse_jsoncons)
        if(declared OR synapse_jsoncons_POPULATED OR FETCHCONTENT_SOURCE_DIR_SYNAPSE_JSONCONS)
            message(FATAL_ERROR "Pre-existing jsoncons declaration/source override has unqualified provenance/correction")
        endif()
        FetchContent_Declare(synapse_jsoncons
            SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/synapse_jsoncons-src"
            SOURCE_SUBDIR synapse-no-upstream-cmake
            DOWNLOAD_COMMAND "${CMAKE_COMMAND}" "-DJSONCONS_SOURCE_DIR=<SOURCE_DIR>"
                -DJSONCONS_FETCH_ONLY=ON -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ApplyJsonconsPatch.cmake"
            PATCH_COMMAND "${CMAKE_COMMAND}" "-DJSONCONS_SOURCE_DIR=<SOURCE_DIR>"
                -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ApplyJsonconsPatch.cmake")
        FetchContent_MakeAvailable(synapse_jsoncons)
        set(source "${synapse_jsoncons_SOURCE_DIR}")
        add_library(jsoncons INTERFACE)
        add_library(jsoncons::jsoncons ALIAS jsoncons)
        target_include_directories(jsoncons SYSTEM INTERFACE "${source}/include")
        target_compile_options(jsoncons INTERFACE
            "$<$<AND:$<PLATFORM_ID:Windows>,$<CXX_COMPILER_ID:GNU>>:-Wa,-mbig-obj>"
            "$<$<AND:$<PLATFORM_ID:Windows>,$<CXX_COMPILER_ID:MSVC>>:/bigobj>")
        set_property(TARGET jsoncons PROPERTY SYNAPSE_JSONCONS_SOURCE_DIR "${source}")
        set_property(TARGET jsoncons PROPERTY SYNAPSE_JSONCONS_COMMIT "9e57e421285422d7aeeb5aa7dc839f6a19121ee6")
        set_property(TARGET jsoncons PROPERTY SYNAPSE_JSONCONS_QUALIFIED TRUE)
        set_property(GLOBAL PROPERTY SYNAPSE_OWNED_JSONCONS TRUE)
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DJSONCONS_SOURCE_DIR=${source}"
        -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ApplyJsonconsPatch.cmake" RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "jsoncons dependency correction qualification failed")
    endif()
    synapse_add_srell_dependency()
    target_link_libraries(jsoncons INTERFACE srell::srell)
endfunction()
