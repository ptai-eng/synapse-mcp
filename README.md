# Synapse MCP

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

**Synapse MCP** is a modern, lightweight, header-only C++23 SDK for the [Model Context Protocol (MCP)](https://modelcontextprotocol.io/). It empowers C++ applications to expose local tools, resources, and system functionality directly to LLM Agents (like Claude Desktop, Cursor, and Gemini) with near-zero overhead.

---

## Highlights

* **Header-only:** Just copy the `include/` folder or use CMake `FetchContent`. No static/dynamic linking or build-system headaches.
* **Modern C++ Design:** Leverages C++23 `std::expected` for elegant, exception-free functional error handling, and `std::jthread` for leak-free background I/O.
* **Minimalist Dependencies:** Built solely on top of the industry-standard `nlohmann/json`. No heavy frameworks like Boost or custom networking engines.
* **Non-blocking STDIO Transport:** Lightning-fast JSON-RPC 2.0 communication over Standard I/O—the exact transport required for local desktop MCP integrations.
* **Expressive API:** Exposing C++ business logic feels natural. Register tools and resources using simple C++ lambdas.

## Usage Example

Writing an MCP server should be effortless. Here is a complete example of exposing a C++ function as a tool to an AI agent:

```cpp
#include <synapse/mcp.hpp>
#include <iostream>

int main() {
    // Initialize the server over standard input/output (STDIO)
    synapse::server server;

    // 1. Register a Tool
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
        [](const nlohmann::json& args) -> std::expected<nlohmann::json, synapse::error> {
            try {
                std::string text = args.at("text").get<std::string>();
                std::string hash = compute_sha256(text); // User logic
                return nlohmann::json{{"hash", hash}};
            } catch (const std::exception& e) {
                return std::unexpected(synapse::error{synapse::error_code::invalid_params, e.what()});
            }
        }
    );

    // 2. Start the server (non-blocking std::jthread)
    server.start();
    
    // 3. Wait for shutdown or EOF on stdin
    server.wait();
    return 0;
}
```

## Integration

### Using CMake `FetchContent` (Recommended)

The easiest way to integrate Synapse MCP is via CMake. It will automatically download the library and its `nlohmann/json` dependency.

```cmake
include(FetchContent)

FetchContent_Declare(
    synapse_mcp
    GIT_REPOSITORY https://github.com/your-username/synapse-mcp.git
    GIT_TAG main
)
FetchContent_MakeAvailable(synapse_mcp)

add_executable(my_server main.cpp)
target_link_libraries(my_server PRIVATE synapse::mcp)
```

### As a Header-Only Library

If you prefer not to use CMake's FetchContent, you can simply drop the `include/synapse` directory into your project's include path. You will just need to ensure that you also provide `nlohmann/json`.

## Requirements

* A **C++23** compatible compiler (GCC 13+, Clang 16+, MSVC 19.36+)
* **CMake 3.20+** (if using the build system)
* **nlohmann/json 3.11+**

## License

Synapse MCP is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
