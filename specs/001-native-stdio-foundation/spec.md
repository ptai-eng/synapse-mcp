# Specification 001: A dependable native stdio integration

- Status: Accepted scope; implementation not started
- Created: 2026-10-25
- Milestone: M1, the first verified native stdio server integration
- Decision: [ADR-001](../../docs/decisions/0001-product-and-protocol.md)
- Principles: [Constitution](../../.specify/memory/constitution.md)

## Objective and primary user

Help a C++ application or tooling developer expose an existing native module's operations and state to an MCP host without moving that domain logic into another language or application framework.

The primary integration is a host-launched local executable linking the domain module and Synapse, with stdin/stdout dedicated to MCP. C++23 and a compatible standard library are required; the core remains header-only.

Secondary audiences are developers building MCP clients or network adapters. Those APIs remain experimental while M1 verifies the server path.

### First reference scenario

Provide a small native application whose application-owned state includes a monotonic start time and a count of handled operations. A read-only `app.status` tool and an `app://status` resource expose a snapshot computed from that state; a registered `explain_status` prompt helps a host explain it. Tests control the state/clock to make assertions deterministic. The executable must report actual application state, rather than dummy hashes or fixed CPU/memory readings.

An independent client launches the executable, negotiates `2025-11-25`, discovers the registered surface, calls the status tool, reads the resource and closes the connection. The example establishes a reusable embedding pattern, not a system-monitoring product.

## User stories

1. As the owner of a native module, I can link a small SDK target, register application handlers before serving and access the module through a local MCP host.
2. As an integrator, I can identify the negotiated revision, supported capabilities and callback execution context from the API and documentation.
3. As an application owner, I can close the connection and release resources without a worker accessing destroyed state or input parsing terminating the process.

## Selected protocol profile

The normative target is **MCP `2025-11-25`**. This is a dated profile, not a claim to implement every optional MCP feature. All target rows below are **planned** until the release gates pass. Use the selected revision's specifications when implementing each row.

| Area | M1 scope | Explicit limit |
|---|---|---|
| Lifecycle | `initialize`, version counteroffer, capability/identity exchange, `notifications/initialized`, then operation | No 2024 fallback or modern discovery negotiation |
| Stdio | UTF-8 JSON-RPC messages delimited by newlines; clean stdout; diagnostics on stderr; EOF and stop handling | No HTTP server or network authentication |
| Protocol boundary | Validate envelopes, IDs, methods and parameters; correct error/notification behavior; reply to `ping` | No claim that serialization alone proves conformance |
| Tools | Static registration; `tools/list` and `tools/call`; declared input schema and validation; text results and execution errors | No list-change notifications, async task capability or untested content type |
| Resources | Static registration; list/read text resources, URI and MIME metadata; empty template list when no templates exist | No subscriptions, template expansion or list-change notifications |
| Prompts | When registered: list/get with validated arguments and text message content | No completion or untested multimodal content |
| Other capabilities | Only capabilities exercised by the supported profile are advertised | Sampling, roots, elicitation, protocol logging and extensions are outside M1 |

Tool inputs must be checked against the declared contract before domain operations execute. Require JSON Schema 2020-12 support, honor an explicitly declared dialect, and use 2020-12 when none is declared. Handle unsupported dialects with a clear error rather than executing unvalidated input. The implementation plan must select the validation mechanism before changing tool execution; this decision step does not select a validator dependency or establish implemented support. Any unmet normative requirement is a release blocker, rather than something hidden by a feature-profile label. [Selected base protocol](https://modelcontextprotocol.io/specification/2025-11-25/basic/index).

Handle parse/envelope/unknown-method failures, malformed call parameters and unknown tools as protocol errors. For example, a string where `arguments` must be an object is a malformed call; an arguments object violating the tool's declared schema or domain rules is a tool execution failure and returns `isError: true`. Apply the same execution-error channel to application failures. Unknown notifications receive no response. Keep errors attributable to the protocol, transport or application boundary.

The selected tools specification also requires access controls, invocation rate limits and output sanitization. The implementation plan must define and test appropriate local policies; stdio and the absence of OAuth do not waive these requirements. [Tool security requirements](https://modelcontextprotocol.io/specification/2025-11-25/server/tools#security-considerations).

References: [Lifecycle](https://modelcontextprotocol.io/specification/2025-11-25/basic/lifecycle), [Transports](https://modelcontextprotocol.io/specification/2025-11-25/basic/transports), [Tools](https://modelcontextprotocol.io/specification/2025-11-25/server/tools), [Resources](https://modelcontextprotocol.io/specification/2025-11-25/server/resources), [Prompts](https://modelcontextprotocol.io/specification/2025-11-25/server/prompts), [Ping](https://modelcontextprotocol.io/specification/2025-11-25/basic/utilities/ping).

## Ownership and execution constraints

Registration completes before serving; runtime registry mutation is outside the first supported profile. Define the callback execution context and the application's responsibility for domain-state access in the implementation plan. Do not infer application thread safety from an SDK mutex.

SDK-owned I/O and work must have deterministic owners. Stop accepting work before draining/cancelling queued work and releasing callback state. Active callbacks cooperate with cancellation or return normally; the SDK cannot safely force arbitrary application code to stop. Tests must distinguish blocked transport I/O from non-cooperative callbacks.

## Acceptance criteria

| ID | Observable result | Required evidence |
|---|---|---|
| AC-01 | A fresh downstream CMake project links `synapse::mcp` and builds the documented native example without setting C++23 manually on its own target | Clean consumer configure/build transcript |
| AC-02 | A client pinned to `2025-11-25` completes initialization and sends the initialized notification; unsupported-version tests never falsely echo the offered version | Protocol transcript and public-boundary tests |
| AC-03 | Tool discovery/call, resource discovery/read and a registered prompt return the documented native state/content | End-to-end assertions using a deterministic application fixture |
| AC-04 | Malformed JSON, malformed valid-JSON envelopes, wrong argument types, unknown methods and failing callbacks produce the specified behavior without crashing the server | Negative protocol and process-level tests |
| AC-05 | EOF and explicit stop interrupt SDK-owned input waits; no worker outlives its owner; cooperative active work and pending operations settle | Bounded process tests, repeated lifecycle/resource checks and ownership review |
| AC-06 | At least one independent official SDK client exercises the exact revision over stdio, and one named MCP host completes the reference flow | Exact package/host, compiler, standard library and OS versions plus the negotiated transcript; record whether client and host are the same implementation |
| AC-07 | The declared Linux and Windows toolchain configurations pass the supported profile in CI; other platforms remain unverified | CI matrix and recorded toolchain versions, pinned during implementation planning |
| AC-08 | README installation and example instructions reproduce the reference flow; status matches the evidence | Reproduction from a clean checkout; supported/experimental capability matrix |

For AC-05, use a two-second subprocess exit budget after EOF for the reference app with idle or cooperative handlers. A deliberately non-cooperative handler must not trigger unsafe forced destruction; its documented shutdown limitation needs a separate test. This budget is an integration test threshold, not a universal execution deadline for application code.

For AC-06, use the official TypeScript client in legacy mode with its supported revision list containing only `2025-11-25`; pin an exact released package version in the test harness. Inspector can help inspect transcripts, but selecting its legacy era alone does not establish the exact negotiated revision. Host compatibility remains unverified until this evidence exists. [Official client implementation](https://github.com/modelcontextprotocol/typescript-sdk/blob/main/packages/client/src/client/client.ts), [Inspector eras](https://modelcontextprotocol.io/docs/2026-07-28/tools/inspector/protocol-eras).

## Commands and current verification baseline

Run from the repository root with the intended compiler environment configured:

```sh
cmake -S . -B build -DSYNAPSE_MCP_BUILD_EXAMPLES=ON -DSYNAPSE_MCP_BUILD_TESTS=ON
cmake --build build --config Debug --parallel 4
ctest --test-dir build -C Debug --output-on-failure
```

These commands describe the existing build/test entry points. At assessed commit `22c2126`, an existing Windows MinGW build and three serialization cases with 18 assertions passed. They do not satisfy the new acceptance criteria or establish current host compatibility. The next implementation plan must add the independent-client and clean-consumer commands before calling M1 verified.

## Project structure and code conventions

```text
include/synapse/              Public headers, protocol and transport code
include/nlohmann/             Vendored JSON dependency
examples/                    Runnable integrations and their CMake targets
tests/                       Catch2 tests; integration fixtures added during M1
docs/decisions/               Accepted decisions and their rationale
specs/001-native-stdio-foundation/  This milestone's scope and acceptance
.specify/memory/              Project constitution
.understand-anything/         Local analysis artifacts, outside product commits
```

Preserve the `synapse` namespace and existing snake_case conventions while the API is refined. Use concrete error payloads rather than an error-code enum where context is needed. For example, the existing callback convention is:

```cpp
using tool_callback = std::function<
    std::expected<nlohmann::json, error>(const nlohmann::json&)>;
```

This documents the present naming/error style; it does not freeze the v0.2 callback signature or settle the new result API. Keep protocol conversion and transport details separate from application handlers.

## Testing strategy

Use Catch2 for public-boundary behavior and protocol fixtures. Add subprocess tests for framing, malformed input, EOF and shutdown. Use a fresh consumer project for packaging and an independent official SDK client for wire interoperability; self-to-self tests are supplementary. Add regression tests before complex runtime fixes. Use sanitizer/resource checks where supported, and document unavailable checks.

## Boundaries and implementation handoff

- Always: keep target and verified status separate, trace changes to acceptance criteria, validate capability claims, and document ownership.
- Within delegated scope: choose implementation details, dependencies and CI changes after recording their trade-offs and verification in the implementation plan.
- Never: commit local analysis archives, modify vendored internals for a Synapse fix, replace user data, publish untested support claims or treat v0.2 examples as release evidence.

M1 excludes a general agent framework, model hosting, cloud deployment, HTTP/OAuth, universal GUI integration, C++20 fallback, dual-era support and performance superiority claims.

The next step is an implementation plan and behavior-driven task list for M1. That plan must settle execution/cancellation, schema validation, local access/rate/output policies, exact test toolchains and harness package versions before code changes. This step accepts the product and protocol decisions only; it does not mark those implementation choices or acceptance tests complete.
