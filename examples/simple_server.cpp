#include <synapse/mcp.hpp>
#include <iostream>

int main() {
    const std::string greeting = "Hello from Synapse";
    synapse::stdio_transport transport;
    synapse::server server(transport);
    auto registered = server.register_resource(
        {"app://greeting", "Greeting", "A basic text resource", "text/plain"},
        [&](const synapse::request_context&) -> std::expected<synapse::resource_result, synapse::handler_error> {
            return synapse::resource_result{greeting};
        });
    if (!registered || !server.start()) { std::cerr << "Cannot start server\n"; return 1; }
    if (!server.wait()) { std::cerr << "Cannot join server\n"; return 1; }
}
