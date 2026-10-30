# Synapse MCP

Expose native application state and operations to an MCP host from C++23. Synapse provides a **header-only server core** for local, host-launched **stdio** integrations targeting MCP **2025-11-25**.

The source on `master` prepares **v0.3.0**, the M1 native stdio foundation. This is a pre-1.0 SDK with breaking server API changes from v0.2; review the [migration guide](docs/migration/v0.2-to-m1.md) and [release notes](docs/releases/v0.3.0.md) before upgrading. Integration uses CMake source-tree embedding; no installed `find_package` export or package-manager distribution is provided.

The reproducible Quick Start below pins the previously qualified runtime commit `1fa654b`, which reports `0.2.0`. It reproduces the historical acceptance evidence. Release-specific verification and publication status are tracked in the release notes.

To evaluate the v0.3.0 candidate from a fresh clone, replace the detached-checkout command below with `git -C "Synapse checkout" checkout master` and record `git -C "Synapse checkout" rev-parse HEAD` with your results. Once published, use `checkout --detach v0.3.0` for an immutable release checkout. The build and embedding commands are otherwise the same; historical evidence remains tied to its recorded commit.

## Runtime architecture

A host launches the application and exchanges UTF-8 newline-delimited JSON-RPC over stdin/stdout. Native transport code frames input and bounds buffered output; the server validates envelopes and initialization state before dispatch. Registered application handlers and policy hooks execute on one owned worker. The server owns that work but borrows the transport and application state.

| Boundary | Contract | Implementation |
|---|---|---|
| Transport | Bounded frames; interruptible SDK-owned I/O on Windows pipes and POSIX streams | [Transport](include/synapse/transport.hpp) |
| Protocol and lifecycle | MCP `2025-11-25`; register before start; typed errors; explicit stop/join | [Server](include/synapse/server.hpp) |
| Application execution | One owned worker; bounded queue; cooperative cancellation | [Executor](include/synapse/detail/owned_executor.hpp) |
| Tool inputs | Compile schemas at registration; validate arguments before callbacks | [Schema adapter](include/synapse/detail/schema_validator.hpp) |
| Local policy | Queue/rate limits, authorization hooks and output checks; application supplies domain rules | [Options](include/synapse/server_options.hpp) |

These boundaries support embedding in an existing native application. Applications must synchronize their own shared state; non-cooperative callbacks can delay shutdown. See the lifecycle requirements in the embedding example below.

## Quick Start: build the native reference

You need Git, CMake 3.20+ and a C++23 compiler/standard library with `std::expected`. CMake uses checked-in nlohmann/json 3.11.3 and fetches pinned jsoncons/SRELL sources with the qualified schema correction. The first uncached configure needs network access. Tests also fetch pinned Catch2 and schema fixtures. See [dependency versions, correction and notices](docs/third-party.md).

**Windows PowerShell**, with Visual Studio 2022 C++ tools v143 14.44 and Windows SDK 10.0.26100.0 installed:

```powershell
git clone --no-checkout https://github.com/ptai-eng/synapse-mcp.git "Synapse checkout"
git -C "Synapse checkout" checkout --detach 1fa654be73d582e9ae4778da0d757e9112a6bac1
cmake -S "Synapse checkout" -B "Synapse reference build" -G "Visual Studio 17 2022" -A x64 -T "v143,version=14.44" "-DCMAKE_SYSTEM_VERSION=10.0.26100.0" -DSYNAPSE_MCP_BUILD_EXAMPLES=ON -DSYNAPSE_MCP_BUILD_TESTS=OFF
cmake --build "Synapse reference build" --config Debug --target native_status --parallel 2
```

The executable is `Synapse reference build/examples/Debug/native_status.exe`. An MCP host launches it with no arguments and pipe-backed stdin/stdout. Use its absolute path in your host's stdio configuration. Stdout carries UTF-8 newline-delimited JSON-RPC; send diagnostics to stderr. Running it in a human console is not an interactive demonstration.

[Native reference source](examples/native_status.cpp) exposes application-owned state through:

| MCP entry | Behavior |
|---|---|
| Tool `app.status` | Increments the status-tool invocation counter and returns the current snapshot. |
| Resource `app://status` | Reads the same snapshot without incrementing the counter. |
| Prompt `explain_status` | Returns a user message containing that snapshot. |

Snapshots include uptime, the counter and its scope. See the [official-client reproduction](docs/verification/m1.md#clean-checkout-reference-and-consumer) for a complete initialize/discover/call/read/get flow and the [Inspector Web evidence](docs/verification/m1.md#inspector-web-session) for the verified host session.

**Linux shell**, with the qualified GCC 14.4/libstdc++ environment and Ninja available, from the directory containing the checkout:

```sh
cmake -S "Synapse checkout" -B "Synapse Linux build" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSYNAPSE_MCP_BUILD_EXAMPLES=ON -DSYNAPSE_MCP_BUILD_TESTS=OFF
cmake --build "Synapse Linux build" --target native_status --parallel 2
```

Linux produces `Synapse Linux build/examples/native_status`. The [CI workflow](.github/workflows/ci.yml) pins the exact qualified container and tool setup. A different compiler or standard library requires separate qualification.

## Embed in your application

Add the source checkout as a subdirectory and link the interface target. It propagates C++23, Threads and required headers. SDK examples/tests default OFF when embedded; they default ON in a top-level SDK build.

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_app LANGUAGES CXX)
add_subdirectory("../Synapse checkout" synapse-sdk)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE synapse::mcp)
```

Put this in `main.cpp` beside your application's `CMakeLists.txt`:

```cpp
#include <synapse/server.hpp>

int main() {
    std::string application_text = "ready";
    synapse::stdio_transport transport;
    synapse::server server(transport, {"my-app", "1.0.0"});
    auto registered = server.register_tool(
        {"app.status", "Read application state",
         nlohmann::json{{"type", "object"}, {"additionalProperties", false}}},
        [&application_text](const nlohmann::json&, const synapse::request_context&)
            -> std::expected<synapse::tool_result, synapse::handler_error> {
            return synapse::tool_result{application_text};
        });
    if (!registered || !server.start()) return 1;
    if (!server.wait()) return 1;
}
```

Register methods before `start()`. Keep captured application state and the borrowed transport alive through server destruction and joined work; the declaration order above does that. Construct/destroy the server on its owner thread and coordinate external access to your state. Handlers and policy hooks run on one application worker.

For long work, poll `request_context.stop.stop_requested()`. `stop()` requests cancellation and interrupts SDK-owned I/O; the owner calls `wait()` to join. A non-cooperative callback can delay destruction until it returns. A timed `wait_for()` returning `false` still requires live state, transport and server. The [migration guide](docs/migration/v0.2-to-m1.md) covers callback restrictions, typed errors and the v0.2 API changes, including removed reverse server requests.

To check the supplied downstream consumer, use **Windows PowerShell** from the directory containing the checkout:

```powershell
cmake -S "Synapse checkout/tests/consumer" -B "Synapse consumer build" -G "Visual Studio 17 2022" -A x64 -T "v143,version=14.44" "-DCMAKE_SYSTEM_VERSION=10.0.26100.0"
cmake --build "Synapse consumer build" --config Debug --parallel 2
& ".\Synapse consumer build\Debug\synapse_consumer.exe"
```

This fixture checks source-tree linking, propagation of C++23 from a C++17 parent, dependency include roots and schema behavior.

## Verified scope and limits

The [verification record](docs/verification/m1.md) ties results to runtime source `1fa654be73d582e9ae4778da0d757e9112a6bac1` and records exact environments and limits:

| Environment | Recorded result |
|---|---|
| Windows, VS 2022/v143 14.44, MSVC 19.44, SDK 10.0.26100.0 | Hosted suite: 118/118 checks passed. |
| Linux, pinned GCC 14.4.0/libstdc++ container | Functional, ASan+UBSan and TSan jobs: 117/117 checks passed per job. |
| Official MCP client 2.4.0, Node 24.22.1 | Windows/Linux protocol and application flow passed. |
| MCP Inspector Web 2.12.0 in Edge on Windows | User-operated tool/resource/prompt walkthrough passed with `2025-11-25` negotiation. |

Inspector and the official-client harness share TypeScript SDK lineage. Other hosts, including Codex, Claude Desktop and VS Code, and other OS/compiler combinations remain unverified. No performance advantage is claimed.

The server negotiates exactly `2025-11-25` and supports registered tools, resources and prompts with text-only results; resource-template listing returns empty. HTTP/OAuth, subscriptions, list-change notifications, sampling, roots, elicitation, completion and protocol logging are outside this runtime's scope. Client, subprocess and SSE APIs remain experimental. MCP `2026-07-28` requires a separate migration.

Per-server queue/rate limits, tool authorization hooks and output filtering provide local controls. Applications must supply domain authorization and redaction; built-in text checks do not detect secrets or prompt injection. Read the [policy and shutdown contract](docs/migration/v0.2-to-m1.md) before integrating application code.

## Security scan and reviewed warnings

SkillSpector 2.3.11 scanned source commit `184e406` on 2026-10-29 with `--no-llm` and no suppression baseline. Its original result is **85/100, CRITICAL, DO_NOT_INSTALL**. The report contains **0 Critical, 55 High, 143 Medium and 2 Low findings**: the Critical label is the aggregate score, not an individually confirmed Critical vulnerability.

Review of all 200 retained matches classified 199 as false positives for the behavior alleged: dependency names containing `keyring`, C++ formatting/lookup tables, ordinary allocation code, documentation, and `--no-checkout` matching a `--no-check` pattern. This does not certify those dependencies or code as vulnerability-free.

**One High warning is a real CI isolation limitation:** the TSan container uses `seccomp=unconfined`. It applies only to that GitHub Actions sanitizer job, not the SDK's runtime or normal build. The setting is retained and remains a hardening item; it is not dismissed as a false positive. Do not copy it into a production deployment.

Read the [scope, per-rule triage and reproduction instructions](docs/security/skillspector-2026-10-29.md) alongside the [machine report](docs/security/evidence/skillspector-2026-10-29.json). The scan's `is_complete` is `false`; LLM analysis was disabled, and this agent-skill scanner is not a complete C++ security audit. Its original score has not been suppressed or replaced with a clean badge.

## Development and contributing

For the complete C++ suite, use **Windows PowerShell** after the reference configure above:

```powershell
cmake -S "Synapse checkout" -B "Synapse reference build" -DSYNAPSE_MCP_BUILD_TESTS=ON
cmake --build "Synapse reference build" --config Debug --parallel 2
ctest --test-dir "Synapse reference build" -C Debug --output-on-failure
```

Official-client tests require the pinned Node/npm tools and `SYNAPSE_MCP_BUILD_INTEROP_TESTS=ON`; follow the [focused interop commands](docs/verification/m1.md#clean-checkout-reference-and-consumer). Build the relevant targets before invoking CTest. The hosted matrix covers the unchanged runtime; the verification record distinguishes fresh documentation reproductions from full regression runs.

Start with the [M1 specification](specs/001-native-stdio-foundation/spec.md), [product/protocol decision](docs/decisions/0001-product-and-protocol.md) and [constitution](.specify/memory/constitution.md). Add behavioral tests for runtime changes and record scope decisions. The [implementation checklist](tasks/todo.md) tracks development work.

The [README reproduction record](docs/verification/readme-2026-10-29.md) records the clean-checkout checks and remaining platform limits.

## License

Synapse MCP uses the [MIT License](LICENSE). Dependencies retain their own licenses; preserve the applicable [third-party notices](docs/third-party.md) when distributing source or binaries.
