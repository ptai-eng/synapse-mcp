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

add_subdirectory(schema)

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
