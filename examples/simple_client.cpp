#include <iostream>
#include <synapse/client.hpp>
#include <synapse/process_transport.hpp>
#include <thread>
#include <chrono>

int main() {
    try {
        std::cout << "Starting Synapse MCP Client...\n";

        // Create the transport to run the simple_server
#ifdef _WIN32
        synapse::process_transport transport("simple_server.exe", {});
#else
        synapse::process_transport transport("./simple_server", {});
#endif

        // Create the client
        synapse::client client(transport);

        // Send initialize request
        std::cout << "Sending initialize request...\n";
        auto init_future = client.initialize();
        auto init_result = init_future.get();

        if (!init_result) {
            std::cerr << "Initialization failed: " << init_result.error().message << "\n";
            return 1;
        }
        std::cout << "Initialized successfully!\nServer Info: " << init_result.value().dump() << "\n\n";

        // Call a tool (calculator)
        std::cout << "Calling tool 'calculator' with 5 + 7...\n";
        auto tool_future = client.call_tool("calculator", {
            {"operation", "add"},
            {"a", 5},
            {"b", 7}
        });
        auto tool_result = tool_future.get();

        if (!tool_result) {
            std::cerr << "Tool call failed: " << tool_result.error().message << "\n";
        } else {
            std::cout << "Tool call result: " << tool_result.value().dump(2) << "\n";
        }

        std::cout << "Client finished. Shutting down...\n";
        transport.stop();
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    }
}
