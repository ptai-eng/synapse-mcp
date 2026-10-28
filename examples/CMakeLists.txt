add_executable(simple_server simple_server.cpp)
target_link_libraries(simple_server PRIVATE synapse::mcp)

add_executable(simple_client simple_client.cpp)
target_link_libraries(simple_client PRIVATE synapse::mcp)

add_executable(native_status native_status.cpp)
target_link_libraries(native_status PRIVATE synapse::mcp)
