include_guard(GLOBAL)

# The test data is identified by an exact Git commit, including its remote
# fixtures. This supplies data to tests, never a runtime URI resolver.
function(synapse_use_schema_test_suite target)
    set(_suite_pin "89e83a74014617ffbe79a56d83d14d054cd11417")
    set(SYNAPSE_SCHEMA_TEST_SUITE_SOURCE_DIR
        "${CMAKE_BINARY_DIR}/_deps/synapse_schema_suite-src" CACHE PATH
        "Checkout of the pinned official JSON Schema Test Suite")
    find_package(Git REQUIRED QUIET)
    if(NOT EXISTS "${SYNAPSE_SCHEMA_TEST_SUITE_SOURCE_DIR}/.git")
        if(EXISTS "${SYNAPSE_SCHEMA_TEST_SUITE_SOURCE_DIR}")
            message(FATAL_ERROR "Schema suite directory lacks verifiable Git provenance")
        endif()
        file(MAKE_DIRECTORY "${SYNAPSE_SCHEMA_TEST_SUITE_SOURCE_DIR}")
        foreach(_arguments IN ITEMS "init" "config;core.autocrlf;false"
                "remote;add;origin;https://github.com/json-schema-org/JSON-Schema-Test-Suite.git"
                "fetch;--depth;1;origin;${_suite_pin}" "checkout;--detach;FETCH_HEAD")
            execute_process(COMMAND "${GIT_EXECUTABLE}" ${_arguments}
                WORKING_DIRECTORY "${SYNAPSE_SCHEMA_TEST_SUITE_SOURCE_DIR}"
                RESULT_VARIABLE _result OUTPUT_VARIABLE _output ERROR_VARIABLE _error)
            if(NOT _result EQUAL 0)
                message(FATAL_ERROR "Cannot prepare pinned schema suite: ${_output}${_error}")
            endif()
        endforeach()
    endif()
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
        WORKING_DIRECTORY "${SYNAPSE_SCHEMA_TEST_SUITE_SOURCE_DIR}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _revision OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT _result EQUAL 0 OR NOT _revision STREQUAL _suite_pin)
        message(FATAL_ERROR "JSON Schema Test Suite commit does not match ${_suite_pin}")
    endif()
    execute_process(COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=all
        WORKING_DIRECTORY "${SYNAPSE_SCHEMA_TEST_SUITE_SOURCE_DIR}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _changes OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT _result EQUAL 0 OR NOT _changes STREQUAL "")
        message(FATAL_ERROR "JSON Schema Test Suite checkout differs from its pinned content")
    endif()
    if(NOT EXISTS "${SYNAPSE_SCHEMA_TEST_SUITE_SOURCE_DIR}/tests/draft2020-12/vocabulary.json"
            OR NOT EXISTS "${SYNAPSE_SCHEMA_TEST_SUITE_SOURCE_DIR}/remotes/draft2020-12/tree.json")
        message(FATAL_ERROR "Pinned schema suite is incomplete")
    endif()
    target_compile_definitions(${target} PRIVATE
        "SYNAPSE_SCHEMA_TEST_SUITE_DIR=\"${SYNAPSE_SCHEMA_TEST_SUITE_SOURCE_DIR}\"")
endfunction()
