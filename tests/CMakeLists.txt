include(FetchContent)

FetchContent_Declare(
    Catch2
    URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.4.0.tar.gz
)
FetchContent_MakeAvailable(Catch2)

add_executable(synapse_tests main.cpp)
target_link_libraries(synapse_tests PRIVATE synapse::mcp Catch2::Catch2WithMain)
add_test(NAME synapse_tests COMMAND synapse_tests)
