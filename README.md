<div align="center">
  <h1>Synapse MCP</h1>
  <p><strong>A C++23 SDK for exposing native application capabilities through MCP</strong></p>
  <p>
    <img src="https://img.shields.io/badge/status-experimental-orange.svg?style=flat-square" alt="Experimental">
    <img src="https://img.shields.io/badge/C%2B%2B-23-blue.svg?style=flat-square&logo=c%2B%2B" alt="C++23">
    <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue.svg?style=flat-square" alt="MIT License"></a>
  </p>
</div>

Synapse MCP helps C++ application and tooling developers expose existing native operations and data to an MCP host. The first supported integration will be a local, host-launched **stdio server**, with a **header-only C++23 core**.

## Project status

The product direction is accepted. The runtime migration and verification are pending.

| Area | Status |
|---|---|
| Current v0.2 implementation | Experimental; partially targets MCP `2024-11-05`, with known lifecycle and integration defects |
| First verified milestone, M1 | Targets exactly MCP `2025-11-25` over stdio; not implemented or certified yet |
| MCP `2026-07-28` | Separate future migration, outside M1 |
| Client, subprocess and SSE APIs | Experimental; no production-support commitment |

Protocol target, implemented behavior and verified interoperability are separate claims. Existing serialization tests do not certify a complete server integration. No host/platform compatibility or performance advantage is claimed without recorded evidence.

## Product and protocol decisions

- [ADR-001: target users, revision choice and alternatives](docs/decisions/0001-product-and-protocol.md)
- [M1 specification: scope and acceptance criteria](specs/001-native-stdio-foundation/spec.md)
- [Project constitution: engineering principles](.specify/memory/constitution.md)

M1 focuses on a native application exposing a status tool, resource and prompt backed by its own state. Network deployment, model orchestration and dual-era protocol support are outside this milestone. The decision record explains why the first release target differs from the latest specification.

## Development

Use CMake 3.20 or newer and a C++23 compiler/standard library supporting `std::expected`. The current build fetches nlohmann/json; enabling tests also fetches Catch2.

```sh
cmake -S . -B build -DSYNAPSE_MCP_BUILD_EXAMPLES=ON -DSYNAPSE_MCP_BUILD_TESTS=ON
cmake --build build --config Debug --parallel 4
ctest --test-dir build -C Debug --output-on-failure
```

Configure your compiler environment before running CMake. The [server example](examples/simple_server.cpp) and [client example](examples/simple_client.cpp) show experimental APIs; they are not a verified integration walkthrough. A tested downstream installation and host connection guide is part of M1.

## Contributing

Start with the M1 specification and link changes to its acceptance criteria. Record scope changes in a decision record, add behavioral tests for runtime fixes, and distinguish implemented features from planned work.

## License

Synapse MCP is available under the [MIT License](LICENSE).
