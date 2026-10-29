include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

FetchContent_Declare(
    Catch2
    URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.4.0.tar.gz
)
FetchContent_MakeAvailable(Catch2)

add_executable(synapse_tests main.cpp)
target_link_libraries(synapse_tests PRIVATE synapse::mcp Catch2::Catch2WithMain)
list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)
catch_discover_tests(synapse_tests TEST_PREFIX "synapse." PROPERTIES TIMEOUT 10)

add_executable(synapse_contract_tests contracts.cpp)
target_link_libraries(synapse_contract_tests PRIVATE synapse::mcp Catch2::Catch2WithMain)
catch_discover_tests(synapse_contract_tests TEST_PREFIX "synapse." PROPERTIES TIMEOUT 10)

add_executable(synapse_executor_tests owned_executor.cpp)
target_link_libraries(synapse_executor_tests PRIVATE synapse::mcp Catch2::Catch2WithMain)
catch_discover_tests(synapse_executor_tests TEST_PREFIX "synapse." PROPERTIES TIMEOUT 10)

add_executable(synapse_protocol_tests message_decoder.cpp)
target_link_libraries(synapse_protocol_tests PRIVATE synapse::mcp Catch2::Catch2WithMain)
catch_discover_tests(synapse_protocol_tests TEST_PREFIX "synapse." PROPERTIES TIMEOUT 10)

add_subdirectory(schema)

if(UNIX)
    add_executable(synapse_posix_tests posix_io.cpp)
    target_link_libraries(synapse_posix_tests PRIVATE synapse::mcp Catch2::Catch2WithMain)
    catch_discover_tests(synapse_posix_tests TEST_PREFIX "synapse." PROPERTIES TIMEOUT 10)
endif()

if(WIN32)
    add_executable(synapse_windows_tests windows_io.cpp)
    target_link_libraries(synapse_windows_tests PRIVATE synapse::mcp Catch2::Catch2WithMain)
    catch_discover_tests(synapse_windows_tests TEST_PREFIX "synapse." PROPERTIES TIMEOUT 10)
endif()

set(_consumer_arguments
    "-DSYNAPSE_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "-DSYNAPSE_CONSUMER_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/consumer"
    "-DSYNAPSE_CONSUMER_GENERATOR=${CMAKE_GENERATOR}"
    "-DSYNAPSE_CONSUMER_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    "-DSYNAPSE_CONSUMER_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
    "-DSYNAPSE_CONSUMER_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
    "-DSYNAPSE_CONSUMER_CONFIG=$<CONFIG>")
add_test(NAME synapse.M1.consumer.build
    COMMAND "${CMAKE_COMMAND}" ${_consumer_arguments}
        -P "${PROJECT_SOURCE_DIR}/cmake/RunConsumer.cmake")
set_tests_properties(synapse.M1.consumer.build PROPERTIES TIMEOUT 120)
foreach(_dependency IN ITEMS nlohmann jsoncons)
    add_test(NAME synapse.M1.consumer.reject_parent_${_dependency}
        COMMAND "${CMAKE_COMMAND}" ${_consumer_arguments}
            "-DSYNAPSE_CONSUMER_PARENT_DEPENDENCY=${_dependency}"
            -P "${PROJECT_SOURCE_DIR}/cmake/RunConsumer.cmake")
    set_tests_properties(synapse.M1.consumer.reject_parent_${_dependency}
        PROPERTIES TIMEOUT 30)
endforeach()

include("${PROJECT_SOURCE_DIR}/cmake/SchemaTestSuite.cmake")
add_executable(synapse_schema_adapter_tests schema_adapter.cpp)
target_link_libraries(synapse_schema_adapter_tests PRIVATE synapse::mcp Catch2::Catch2WithMain)
synapse_use_schema_test_suite(synapse_schema_adapter_tests)
catch_discover_tests(synapse_schema_adapter_tests TEST_PREFIX "synapse." PROPERTIES TIMEOUT 10)

add_executable(synapse_stdio_tests stdio_transport.cpp)
target_link_libraries(synapse_stdio_tests PRIVATE synapse::mcp Catch2::Catch2WithMain)
catch_discover_tests(synapse_stdio_tests TEST_PREFIX "synapse." PROPERTIES TIMEOUT 10)

add_executable(synapse_server_tests server_lifecycle.cpp server_ownership.cpp)
target_link_libraries(synapse_server_tests PRIVATE synapse::mcp Catch2::Catch2WithMain)
catch_discover_tests(synapse_server_tests TEST_PREFIX "synapse." PROPERTIES TIMEOUT 10)

add_executable(synapse_tool_tests tools.cpp)
target_link_libraries(synapse_tool_tests PRIVATE synapse::mcp Catch2::Catch2WithMain)
catch_discover_tests(synapse_tool_tests TEST_PREFIX "synapse." PROPERTIES TIMEOUT 10)

add_executable(synapse_policy_tests server_policy.cpp)
target_link_libraries(synapse_policy_tests PRIVATE synapse::mcp Catch2::Catch2WithMain)
catch_discover_tests(synapse_policy_tests TEST_PREFIX "synapse." PROPERTIES TIMEOUT 10)

add_executable(synapse_resource_tests resources.cpp)
target_link_libraries(synapse_resource_tests PRIVATE synapse::mcp Catch2::Catch2WithMain)
catch_discover_tests(synapse_resource_tests TEST_PREFIX "synapse." PROPERTIES TIMEOUT 10)

add_executable(synapse_prompt_tests prompts.cpp)
target_link_libraries(synapse_prompt_tests PRIVATE synapse::mcp Catch2::Catch2WithMain)
catch_discover_tests(synapse_prompt_tests TEST_PREFIX "synapse." PROPERTIES TIMEOUT 10)

add_executable(synapse_native_status_tests native_status.cpp)
target_link_libraries(synapse_native_status_tests PRIVATE synapse::mcp Catch2::Catch2WithMain)
catch_discover_tests(synapse_native_status_tests TEST_PREFIX "synapse." PROPERTIES TIMEOUT 10)

if(SYNAPSE_MCP_BUILD_INTEROP_TESTS)
    include("${PROJECT_SOURCE_DIR}/cmake/InteropTools.cmake")
    add_test(NAME synapse.M1.interop.client
        COMMAND "${SYNAPSE_INTEROP_NODE}" "${PROJECT_SOURCE_DIR}/tests/interop/run-client.mjs"
            "$<TARGET_FILE:native_status>" "${SYNAPSE_INTEROP_COMPILER}"
            "$<TARGET_FILE:synapse_interop_environment>" "${PROJECT_SOURCE_DIR}"
            "${PROJECT_BINARY_DIR}/evidence/interop-client.json" "${GIT_EXECUTABLE}")
    set_tests_properties(synapse.M1.interop.client PROPERTIES TIMEOUT 30)
    foreach(_group IN ITEMS evidence provenance cleanup)
        add_test(NAME synapse.M1.interop.${_group}
            COMMAND "${SYNAPSE_INTEROP_NODE}" "${PROJECT_SOURCE_DIR}/tests/interop/harness.test.mjs"
                "$<TARGET_FILE:native_status>" "${SYNAPSE_INTEROP_COMPILER}"
                "$<TARGET_FILE:synapse_interop_environment>" "${PROJECT_SOURCE_DIR}" "${GIT_EXECUTABLE}" "${_group}")
        set_tests_properties(synapse.M1.interop.${_group} PROPERTIES TIMEOUT 30)
    endforeach()
endif()
