#include <synapse/mcp.hpp>
#include <iostream>

std::string compute_sha256(const std::string& input) {
    // Dummy SHA-256 for example purposes
    return "dummy-hash-of-" + input;
}

double get_cpu_usage() {
    return 15.4;
}

double get_free_memory() {
    return 8.2;
}

int main() {
    synapse::stdio_transport transport;
    synapse::server server(transport);

    // 1. Register a simple Tool
    server.register_tool(
        "calculate_hash",
        "Calculates the SHA-256 hash of a given string",
        synapse::schema{
            {"type", "object"},
            {"properties", {
                {"text", {{"type", "string"}, {"description", "The input text"}}}
            }},
            {"required", nlohmann::json::array({"text"})}
        },
        [](const nlohmann::json& arguments) -> std::expected<nlohmann::json, synapse::error> {
            try {
                std::string text = arguments.at("text").get<std::string>();
                std::string hash = compute_sha256(text);
                return nlohmann::json{{"hash", hash}};
            } catch (const std::exception& e) {
                return std::unexpected(synapse::error{synapse::error_code::invalid_params, e.what()});
            }
        }
    );

    // 2. Register a Resource (Static or Dynamic data)
    server.register_resource(
        "file://system_status",
        "Returns the current system health status",
        "System health metrics",
        []() -> nlohmann::json {
            return nlohmann::json{
                {"cpu_usage", get_cpu_usage()},
                {"memory_free_gb", get_free_memory()}
            };
        },
        "application/json"
    );

    // Run the server in a non-blocking background loop using std::jthread
    server.start(); 
    
    // Wait until the server receives a shutdown signal or stdio closes
    server.wait();
    return 0;
}
