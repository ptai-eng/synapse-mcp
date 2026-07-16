# Synapse MCP 🧠

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

**Synapse MCP** is a modern, lightweight, header-only C++23 SDK for building [Model Context Protocol (MCP)](https://modelcontextprotocol.io/) servers. It provides a simple and intuitive API for exposing local tools, resources, and system functionality to LLM Agents (like Claude Code, Cursor, Gemini) with zero runtime bloat.

## Features
- **Header-Only:** Drop it into your `include/` folder or use CMake `FetchContent`. Zero build-system headaches.
- **Modern C++23:** Leverages `std::expected` for elegant error handling and `std::jthread` for leak-free background I/O loops.
- **Zero-Dependency (almost):** Built solely on top of the industry-standard `nlohmann/json` library.
- **Async STDIO Transport:** Robust non-blocking communication via Standard I/O.
- **Elite Developer Experience:** Registering tools and resources feels just like writing standard C++ lambdas.

## Quick Start

### 1. Installation
Because Synapse MCP is header-only, you can easily integrate it using CMake's `FetchContent` or simply by copying the `include/synapse` folder into your project. 

*Requirements:*
- A C++23 compatible compiler (GCC, Clang, or MSVC).
- CMake 3.20+

### 2. Example Server
Here's how simple it is to expose a C++ function as an MCP tool to an LLM:

```cpp
#include <synapse/mcp.hpp>
#include <iostream>

int main() {
    synapse::server server;

    // Register a tool for the LLM
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
                std::string hash = "dummy-hash"; // Your actual hash logic here
                return nlohmann::json{{"hash", hash}};
            } catch (const std::exception& e) {
                return std::unexpected(synapse::error{synapse::error_code::invalid_params, e.what()});
            }
        }
    );

    // Run the server in a non-blocking background loop
    server.start(); 
    
    // Wait until the server receives a shutdown signal or stdio closes
    server.wait();
    
    return 0;
}
```

### 3. Build the Example
```bash
cmake -B build
cmake --build build
```

The resulting executable can be directly registered in your Claude Desktop or Cursor configuration.

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
