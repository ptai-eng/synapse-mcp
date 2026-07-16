add_executable(simple_server simple_server.cpp)
target_link_libraries(simple_server PRIVATE synapse::mcp)

add_executable(simple_client simple_client.cpp)
target_link_libraries(simple_client PRIVATE synapse::mcp)
