# Specification 001: A dependable native stdio integration

- Status: Runtime T01-T19 implemented and reviewed; T20 named-host evidence recorded; M1 acceptance incomplete (AC-08 README pending)
- Created: 2026-10-25
- Milestone: M1, the first verified native stdio server integration
- Decision: [ADR-001](../../docs/decisions/0001-product-and-protocol.md)
- Principles: [Constitution](../../.specify/memory/constitution.md)
- Execution: [implementation plan](../../tasks/plan.md) and [TDD task checklist](../../tasks/todo.md); current acceptance evidence is in the [verification record](../../docs/verification/m1.md)

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

The normative target is **MCP `2025-11-25`**. This is a dated profile, not a claim to implement every optional MCP feature. T01-T19 implement and test the rows below on the declared CI toolchains. The named-host walkthrough is recorded; full milestone acceptance still requires README reproduction; see the [verification record](../../docs/verification/m1.md).

| Area | M1 scope | Explicit limit |
|---|---|---|
| Lifecycle | `initialize`, version counteroffer, capability/identity exchange, `notifications/initialized`, then operation | No 2024 fallback or modern discovery negotiation |
| Stdio | UTF-8 JSON-RPC messages delimited by newlines; clean stdout; diagnostics on stderr; EOF and stop handling | No HTTP server or network authentication |
| Protocol boundary | Validate envelopes, IDs, methods and parameters; correct error/notification behavior; reply to `ping` | No claim that serialization alone proves conformance |
| Tools | Static registration; `tools/list` and `tools/call`; declared input schema and validation; text results and execution errors | No list-change notifications, async task capability or untested content type |
| Resources | Static registration; list/read text resources, URI and MIME metadata; empty template list when no templates exist | No subscriptions, template expansion or list-change notifications |
| Prompts | When registered: list/get with validated arguments and text message content | No completion or untested multimodal content |
| Other capabilities | Only capabilities exercised by the supported profile are advertised | Sampling, roots, elicitation, protocol logging and extensions are outside M1 |

Tool inputs must be checked against the declared contract before domain operations execute. Require JSON Schema 2020-12 support, honor an explicitly declared dialect, and use 2020-12 when none is declared. Handle unsupported dialects with a clear error rather than executing unvalidated input. The implementation uses pinned, corrected jsoncons with SRELL under [ADR-003](../../docs/decisions/0003-schema-validation.md) and [ADR-004](../../docs/decisions/0004-schema-qualification-corrections.md). The mandatory draft2020-12 suite passes on the declared CI platforms; optional format assertions remain outside the default profile. Any unmet normative requirement is a release blocker, rather than something hidden by a feature-profile label. [Selected base protocol](https://modelcontextprotocol.io/specification/2025-11-25/basic/index).

Handle parse/envelope/unknown-method failures, malformed call parameters and unknown tools as protocol errors. For example, a string where `arguments` must be an object is a malformed call; an arguments object violating the tool's declared schema or domain rules is a tool execution failure and returns `isError: true`. Apply the same execution-error channel to application failures. Unknown notifications receive no response. Keep errors attributable to the protocol, transport or application boundary.

The selected tools specification also requires access controls, invocation rate limits and output sanitization. The implementation tests per-server admission/rate policy and UTF-8/control/encoded-size output checks; [migration guidance](../../docs/migration/v0.2-to-m1.md#set-local-policy) explains defaults and application redaction hooks. Stdio and the absence of OAuth do not waive these requirements. [Tool security requirements](https://modelcontextprotocol.io/specification/2025-11-25/server/tools#security-considerations).

References: [Lifecycle](https://modelcontextprotocol.io/specification/2025-11-25/basic/lifecycle), [Transports](https://modelcontextprotocol.io/specification/2025-11-25/basic/transports), [Tools](https://modelcontextprotocol.io/specification/2025-11-25/server/tools), [Resources](https://modelcontextprotocol.io/specification/2025-11-25/server/resources), [Prompts](https://modelcontextprotocol.io/specification/2025-11-25/server/prompts), [Ping](https://modelcontextprotocol.io/specification/2025-11-25/basic/utilities/ping).

## Ownership and execution constraints

Registration completes before serving; runtime registry mutation is outside the first supported profile. Handlers and custom policy hooks execute on one owned application worker. Applications synchronize domain-state access from other threads and keep captured state and borrowed transport alive through the owner-thread server destructor/join. Do not infer application thread safety from an SDK mutex.

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

For AC-06, use the official TypeScript client in legacy mode with its supported revision list containing only `2025-11-25`; pin an exact released package version in the test harness. Inspector can help inspect transcripts, but selecting its legacy era alone does not establish the exact negotiated revision. The recorded Inspector walkthrough now supplies named-host evidence for that exact host/version; it does not establish compatibility with other hosts. [Official client implementation](https://github.com/modelcontextprotocol/typescript-sdk/blob/main/packages/client/src/client/client.ts), [Inspector eras](https://modelcontextprotocol.io/docs/2026-07-28/tools/inspector/protocol-eras).

## Commands and current verification baseline

The [verification record](../../docs/verification/m1.md) provides reproduced clean-checkout/reference/consumer commands, exact hosted toolchains, source attribution and CI links. [Migration guidance](../../docs/migration/v0.2-to-m1.md) documents the typed text-only handlers, removed reverse server requests, lifetime and cooperative shutdown.

At source `1fa654be73d582e9ae4778da0d757e9112a6bac1`, all four hosted jobs pass: Windows 118/118; Linux functional, separate ASan+UBSan and TSan each 117/117. Four intentional hosted negative controls verify unit/schema/consumer/interop gate failures. These results satisfy the recorded runtime/CI scope. The Inspector transcript additionally supplies named-host evidence; README acceptance remains pending. The assessed `22c2126` baseline contained only three serialization cases and remains historical evidence.

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

Preserve the `synapse` namespace and snake_case naming. Public handlers use typed text results and `std::expected<result, handler_error>`, with `request_context` carrying the ID and stop token. Keep `sdk_error` (local SDK failures), wire `error` (RPC failures) and `handler_error` (application failures) distinct. The old v0.2 nested callback aliases no longer define the server API. See [handlers.hpp](../../include/synapse/handlers.hpp) and the [migration guide](../../docs/migration/v0.2-to-m1.md).

## Testing strategy

Use Catch2 for public-boundary behavior and protocol fixtures. Add subprocess tests for framing, malformed input, EOF and shutdown. Use a fresh consumer project for packaging and an independent official SDK client for wire interoperability; self-to-self tests are supplementary. Add regression tests before complex runtime fixes. Use sanitizer/resource checks where supported, and document unavailable checks.

## Boundaries and implementation handoff

- Always: keep target and verified status separate, trace changes to acceptance criteria, validate capability claims, and document ownership.
- Within delegated scope: choose implementation details, dependencies and CI changes after recording their trade-offs and verification in the implementation plan.
- Never: commit local analysis archives, modify vendored internals for a Synapse fix, replace user data, publish untested support claims or treat v0.2 examples as release evidence.

M1 excludes a general agent framework, model hosting, cloud deployment, HTTP/OAuth, universal GUI integration, C++20 fallback, dual-era support and performance superiority claims.

T01-T19 implementation and reviewed CI evidence now exist. T20's Inspector Web 2.12.0 walkthrough was performed manually by the user in Edge after automated browser access was denied; its exported transcript is recorded in the verification page. T21's supporting documentation has a reproduced Windows reference/consumer flow; the user explicitly deferred README edits and its acceptance check. Keep the milestone incomplete until AC-08 has its required README evidence. No release publication or universal host compatibility follows from this update.
