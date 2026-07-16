<div align="center">
  <h1>Synapse MCP</h1>
  <p><strong>A Modern, High-Performance, Header-Only C++20 SDK for Model Context Protocol</strong></p>

  <p>
    <a href="https://github.com/ptai-eng/synapse-mcp"><img src="https://img.shields.io/badge/version-0.1.0-blue.svg?style=flat-square" alt="Version 0.1.0"></a>
    <a href="https://github.com/ptai-eng/synapse-mcp/blob/main/LICENSE"><img src="https://img.shields.io/badge/License-MIT-blue.svg?style=flat-square" alt="License"></a>
    <a href="https://isocpp.org/"><img src="https://img.shields.io/badge/C%2B%2B-20-blue.svg?style=flat-square&logo=c%2B%2B" alt="C++20"></a>
  </p>
</div>

---

**Synapse MCP** is a lightweight, ultra-fast, and deeply integrated **C++20 header-only** library designed to help developers build **Model Context Protocol (MCP)** servers and clients with zero friction. Built strictly on modern **C++** paradigms, it leverages `std::expected` and modern standard library features to provide maximum safety without compromising on the bare-metal performance that **C++** developers expect.

## 🚀 Why Synapse MCP?

Inspired by the design philosophies of industry standards like [spdlog](https://github.com/gabime/spdlog) and [abseil](https://github.com/abseil/abseil-cpp):

* **Header-only:** Just drop the `include/` folder into your project and you're ready to go. No complex build steps or linking headaches.
* **Modern C++20/23:** Utilizes the latest language features (`std::expected`, `std::variant`, `std::optional`) for expressive, safe, and clean code.
* **Zero Overhead Abstractions:** Designed for latency-critical applications where every CPU cycle counts.
* **Dependency Light:** Uses only `nlohmann/json` for robust JSON-RPC 2.0 serialization. 

## 📦 Installation

Since Synapse MCP is header-only, installation is trivial.

### Option 1: CMake FetchContent (Recommended)

Add this to your `CMakeLists.txt`:

```cmake
include(FetchContent)
FetchContent_Declare(
    synapse_mcp
    GIT_REPOSITORY https://github.com/ptai-eng/synapse-mcp.git
    GIT_TAG main
)
FetchContent_MakeAvailable(synapse_mcp)

add_executable(my_server main.cpp)
target_link_libraries(my_server PRIVATE synapse::mcp)
```

### Option 2: Copy-Paste

1. Copy the `include/synapse` directory into your project's include path.
2. Ensure you have `nlohmann/json` available in your project.

## 🛠️ Quick Start

Building an MCP server in modern **C++** is now just a few lines of code:

```cpp
#include <synapse/mcp.hpp>
#include <iostream>

int main() {
    // 1. Initialize the STDIO transport layer
    auto transport = std::make_shared<synapse::stdio_transport>();
    
    // 2. Create the MCP Server instance
    synapse::server mcp_server(transport);
    
    // 3. Register a tool
    mcp_server.register_tool(
        "calculate_hash",
        "Computes a dummy SHA-256 hash for a given input string.",
        R"({
            "type": "object",
            "properties": {
                "input": { "type": "string" }
            },
            "required": ["input"]
        })"_json,
        [](const nlohmann::json& args) -> std::expected<nlohmann::json, synapse::error_code> {
            std::string input = args["input"];
            return nlohmann::json{{"hash", "dummy-hash-" + input}};
        }
    );
    
    // 4. Start listening for incoming requests
    transport->start();
    
    return 0;
}
```

## 📚 Documentation

Detailed documentation on handling Prompts, Resources, and advanced Transport layers (like SSE or WebSockets) is coming soon in the `docs/` folder.

## 🤝 Contributing

We welcome contributions! Whether it's adding new transport layers, improving documentation, or fixing bugs, please feel free to open an issue or submit a Pull Request.

## 📄 License

Synapse MCP is distributed under the MIT License. See `LICENSE` for more information.
