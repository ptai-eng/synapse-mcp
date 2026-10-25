# M1 Native Stdio Implementation Plan

> **For agentic workers:** Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task by task. Track execution in [todo.md](todo.md). Read the specification and referenced ADRs before implementation.

**Goal:** Deliver one verified C++23 native stdio server integration using MCP `2025-11-25`.

**Architecture:** Keep the header-only core, borrowed transport and frozen registries. A separate protocol reader admits bounded work to one owned application worker; an interruptible writer handles encoded frames. Validate tool inputs through a pinned, corrected and qualified schema adapter before invoking application code.

**Tech Stack:** C++23, CMake >=3.20, vendored nlohmann/json 3.11.3, jsoncons 1.10.0 with the recorded correction, Catch2 3.4.0; dev-only Node 24.22.1, official MCP client 2.4.0 and Inspector 2.12.0.

**Spec:** [Specification 001](../specs/001-native-stdio-foundation/spec.md). Decisions: [product](../docs/decisions/0001-product-and-protocol.md), [runtime](../docs/decisions/0002-runtime-ownership.md), [schema](../docs/decisions/0003-schema-validation.md). Principles: [constitution](../.specify/memory/constitution.md).

## Status and execution method

Planning is complete when this document, the task checklist and decision records agree. **No runtime implementation, dependency qualification, host verification or conformance run has happened in this planning step.** All runtime tasks start unchecked.

Use an implementation agent and a fresh reviewer for each task. Serialize edits to shared headers and CMake files; parallelize independent investigations/tests only after their shared interfaces exist. Checkpoints require evidence and review, not repeated authorization for work the user has already delegated. Do not publish releases, push commits or configure a user's global MCP catalog as part of this plan.

## Global constraints

- C++23 and a standard library providing `std::expected`; header-only core and `synapse::mcp` remain the integration contract.
- Exactly MCP `2025-11-25` for M1; current partial `2024-11-05` behavior is not an inherited support guarantee. No modern discovery/dual-era path.
- Static tools, text resources and text prompts; no sampling, roots, protocol logging, HTTP/OAuth or production client expansion.
- Registration finishes before serving. Application state and borrowed transport outlive the server and its owner-thread destructor.
- One application worker; no detached worker holding `this`. Stop requests cancellation; it cannot forcibly terminate arbitrary application code.
- Reference application EOF exit budget: **2 seconds**, for idle/cooperative work. Keep the non-cooperative callback limitation explicit.
- Do not edit tracked vendor headers. Only the bounded, version-locked dependency correction in ADR-003 is allowed; additional corrections require a new recorded decision and tests.
- Passing serialization, a client `close()` call or an Inspector era setting alone proves neither lifecycle correctness nor the exact negotiated revision.

## Review focus

Each condition has a named owning task and must appear in its tests:

1. Embedded NUL in a schema-validated string or key must not truncate regex matching; test default/unsupported dialects too (T01, T05).
2. A host that leaves stdin open or stops reading stdout must not trap SDK-owned I/O after stop; cover the Windows cancel-before-I/O race (T07-T09, T18).
3. A notification, duplicate in-flight ID or pre-initialization operation must not invoke an application handler incorrectly or corrupt another request (T04, T10).
4. A malformed tool call, schema mismatch, denied operation and handler exception must use their distinct, sanitized error paths (T11-T12).
5. SDK transport shutdown escalation must not conceal failure to exit on EOF; capture the negotiated revision and distinguish host application from SDK lineage (T17-T20).

## File boundaries and contracts

| Planned file | Responsibility |
|---|---|
| `include/synapse/protocol.hpp` | Wire models/serialization; preserve lossless string and signed/unsigned integer IDs; null is only a response error ID, never a request ID |
| `include/synapse/errors.hpp` | `sdk_errc`, `sdk_error`, `handler_error`; separate local diagnostics from wire `error` |
| `include/synapse/handlers.hpp` | `request_context` and text-only tool/resource/prompt results and callbacks, as pinned in ADR-002 |
| `include/synapse/server_options.hpp` | Server/stdio limits and application policy hooks |
| `include/synapse/detail/message_decoder.hpp` | `decode_message(const nlohmann::json&) -> expected<incoming_message,response>`; validate before dispatch |
| `include/synapse/detail/schema_validator.hpp` | `compile_schema` and `compiled_schema::validate`, exactly as ADR-003; no backend types in public handlers |
| `include/synapse/detail/owned_executor.hpp` | One worker, bounded FIFO, per-operation cancellation and owner-thread join |
| `include/synapse/detail/native_stdio.hpp` | Platform selection and common move-only byte-I/O contract |
| `include/synapse/detail/stdio_posix.hpp`, `stdio_windows.hpp` | Owned duplicated endpoints and platform-specific cancellation; no application callbacks |
| `include/synapse/transport.hpp` | Framing, bounded JSON parsing, writer queue and transport close notification |
| `include/synapse/server.hpp` | Lifecycle, registries and method routing; no outgoing request/promise subsystem in M1 |
| `examples/native_status.cpp`, `native_status_state.hpp` | Real application state, injectable clock, `app.status`, `app://status`, `explain_status` |
| `tests/consumer/`, `tests/schema/`, `tests/interop/` | Clean consumer, schema qualification and independent/process verification |

`incoming_message` is `variant<request,notification,response>`. Decoder errors carry a complete sanitized response with a valid known ID or null. A server with no outgoing requests ignores unsolicited responses. Extend the existing ID representation with unsigned 64-bit storage where needed; never wrap an unsigned ID into a negative signed number.

Use `sdk_error` for SDK configuration/lifetime/I/O failures, `handler_error` for application failures and `protocol.hpp::error` for JSON-RPC failures. `instance_mismatch` maps to a tool execution error, `engine_failure` to a generic internal error. Do not serialize arbitrary backend/native exception messages.

All registered handlers and user-supplied policy functions execute on the same application worker. The exact hooks are `allow_tool(string_view name,const request_context&) -> bool` and `filter_output(string_view operation,string_view text,const request_context&) -> expected<string,handler_error>`. An empty allow hook admits registered tools only; an empty output hook still applies the core's UTF-8/control/encoded-size checks. Clock injection uses `function<steady_clock::time_point()>`.

Native byte I/O provides `open(input,output) -> expected<native_stdio,sdk_error>`, `read_some(span<char>,stop_token)` and `write_some(span<const char>,stop_token)` returning `expected<size_t,sdk_error>`, plus nonthrowing `request_stop()` and `cancel_pending()`. `open` duplicates endpoints; it never takes ownership of the caller's originals. Windows cancellation retries during owner wait until operation completion; a single cancellation call or `native_handle()` cast is not sufficient. M1 verifies pipe-backed stdio, not arbitrary console/device drivers.

The executor owns a queue of request IDs, stop sources and owned invocations. It exposes `enqueue(id, invocation) -> expected<void,sdk_error>`, `cancel(id) noexcept`, `stop() noexcept`, `wait() -> expected<void,sdk_error>` and `wait_for(milliseconds) -> expected<bool,sdk_error>`. Invocations receive their request context. Define duplicate detection/cancellation around a canonical typed ID key; string `"1"` differs from number `1`.

Server lifecycle is `configuring -> starting -> running -> stopping -> stopped`; connection lifecycle is `await_initialize -> await_initialized -> ready -> closing`. An in-flight duplicate ID closes the connection after one null-ID invalid-request diagnostic and cancels pending work; do not run a second invocation. Reuse after completion is not an advertised server-side history-validation feature. A response becomes committed when accepted by the writer queue; later cancellation cannot retract it.

Frame size excludes the line delimiter. Queue accounting includes the actual encoded bytes and terminating LF; test the exact limit as well as one byte beyond it.

## Build and dependency choices

T01 qualifies the exact jsoncons commit/correction. T05 then runs the mandatory 2020-12 suite and adapter cases. **Do not start tool migration if either gate fails.** A failure requires an explicit dependency/implementation decision, not a skip list or a weaker schema claim.

Pin the official [JSON Schema Test Suite](https://github.com/json-schema-org/JSON-Schema-Test-Suite/commit/89e83a74014617ffbe79a56d83d14d054cd11417) to `89e83a74014617ffbe79a56d83d14d054cd11417`. Load draft2020-12 tests and their remote fixtures into an offline URI catalog. Run all non-optional cases; optional format-assertion behavior is not claimed. Validate schemas against the bundled metaschema before compiling; reject unsupported required vocabularies and never fetch references at request time.

Use the existing vendored nlohmann 3.11.3 header as M1's one JSON source; remove the duplicate unconditional download. Do not silently mix it with an incompatible parent target. jsoncons remains a transitive INTERFACE dependency with its tests disabled and its qualified provenance checked. Preserve MIT/BSL notices in distributions.

Default examples/tests ON only for the top-level project, OFF for a downstream consumer. With the CMake 3.20 floor, compare `CMAKE_SOURCE_DIR` and `PROJECT_SOURCE_DIR`; do not rely on `PROJECT_IS_TOP_LEVEL` from 3.21. Add `target_compile_features(synapse_mcp INTERFACE cxx_std_23)` and use Catch2 test discovery with prefix `synapse.` and unit timeout 10 seconds.

## Verification commands and pins

The task creating an entry point must make its command below real; commands are not evidence until run. Use Ninja for the local single-config build with the chosen compiler environment active:

```sh
cmake -S . -B build/m1 -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSYNAPSE_MCP_BUILD_EXAMPLES=ON -DSYNAPSE_MCP_BUILD_TESTS=ON
cmake --build build/m1 --parallel 4
ctest --test-dir build/m1 --output-on-failure
cmake -S tests/consumer -B build/consumer -G Ninja
cmake --build build/consumer --parallel 4
```

The consumer fixture derives the repository source path from its own directory and declares C++17 in its parent; linking `synapse::mcp` must elevate only the consumer target to C++23 without manually setting its standard. No examples/tests targets may leak into that dependency build.

| Verification component | Pin/policy |
|---|---|
| Node | `24.22.1`; use an official checksum-verified archive under ignored `build/tools/` locally, without replacing the global Node installation |
| SDK client | `@modelcontextprotocol/client` exactly `2.4.0`; imports from package root and `/stdio` |
| Named host | `@modelcontextprotocol/inspector` exactly `2.12.0`, web client; temporary read-only config, no global catalog writes |
| npm dependencies | Exact versions and committed `tests/interop/package-lock.json`; `npm ci --ignore-scripts` |
| SDK negotiation | `versionNegotiation:{mode:'legacy'}`, `supportedProtocolVersions:['2025-11-25']`; assert actual initialize response/notification transcript |
| Timeouts | SDK request 5s, harness 15s, CTest outer 30s; raw graceful EOF exit 2s, measured after EOF rather than process startup |
| Linux | `ubuntu-24.04` job with `gcc:14.4.0@sha256:435c3cbc2ad1c25fbc1684c367808ff0ca2035a1bed56734a40165ab5cd7e3bd`; run native and Node tests inside the same container |
| Windows | `windows-2022`, VS 2022 x64, `-T "v143,version=14.44"`, `-DCMAKE_SYSTEM_VERSION=10.0.26100.0`; record actual compiler patch and runner image |

After T16/T17 create the package and registration, run:

```sh
npm --prefix tests/interop ci --ignore-scripts
cmake -S . -B build/m1 -DSYNAPSE_MCP_BUILD_INTEROP_TESTS=ON
cmake --build build/m1 --parallel 4
ctest --test-dir build/m1 --output-on-failure -R '^synapse\.M1\.(interop|process)\.'
```

The SDK harness wraps the transport to record messages; it does not rely on an invented negotiated-version getter. Its `close()` may escalate to process termination, so raw child tests separately measure graceful EOF and fail if a forced kill was needed. Every child created by a test has scoped watchdog cleanup.

Inspector is a named host/debug application, not proof of compatibility with Codex, Claude Desktop or VS Code. Record its actual negotiated revision, exact version and SDK 2.3.1 lineage; do not call it an independent codec implementation from the SDK harness. Host evidence remains pending until the web walkthrough is actually completed.

Sources for pins and caveats: [Node release](https://github.com/nodejs/node/releases/tag/v24.22.1), [SDK release](https://github.com/modelcontextprotocol/typescript-sdk/releases/tag/v2.4.0), [SDK stdio close](https://github.com/modelcontextprotocol/typescript-sdk/blob/v2.4.0/packages/client/src/client/stdio.ts), [Inspector release](https://github.com/modelcontextprotocol/inspector/releases/tag/2.12.0), [GCC image definition](https://github.com/docker-library/official-images/blob/master/library/gcc), [Windows image](https://github.com/actions/runner-images/blob/main/images/windows/Windows2022-Readme.md).

## Order and checkpoints

Follow [T01-T21](todo.md). Dependencies form these paths:

- T01 -> T02 -> T03; T03 -> T04/T05/T06/T07; T07 -> T08 -> T09.
- T04 + T05 + T06 + T09 -> T10; T05 + T10 -> T11 -> T12.
- T12 -> T13 -> T14; T12 + T13 + T14 -> T15 -> T16 -> T17 -> T18 -> T19 -> T20 -> T21.

At each checkpoint, run the accumulated tests, inspect the diff and record results before advancing:

1. **Dependency/consumer:** T01-T03, corrected engine regression and clean consumer pass.
2. **Boundary/schema/executor:** T04-T06, required schema cases pass; no worker lifetime leak.
3. **I/O:** T07-T09, both supported platform paths and blocked-read/write tests pass.
4. **Methods:** T10-T14, lifecycle, policy and text feature flows pass; schema gate remains green.
5. **Reference/interop:** T15-T18, real native state, exact-revision transcript and raw process checks pass.
6. **Release evidence:** T19-T21, CI/host/docs meet every specification criterion. No publication is included.

## Acceptance traceability and risks

| Spec criterion | Owning tasks |
|---|---|
| AC-01 consumer/C++23 | T02, T19 |
| AC-02 lifecycle/revision | T04, T10, T17 |
| AC-03 native tools/resources/prompts | T11, T13-T15, T17, T20 |
| AC-04 malformed input/failures | T04-T05, T11-T12, T18 |
| AC-05 ownership/EOF/stop | T06-T10, T18 |
| AC-06 independent client/named host | T16-T17, T20 |
| AC-07 Windows/Linux | T07-T09, T19 |
| AC-08 reproducible documentation | T02, T20-T21 |

Schema conformance failure blocks T11. Unsupported pipe cancellation blocks the affected platform claim. A non-cooperative callback may delay destruction; do not detach it to make a timeout pass. An unavailable host or sanitizer is recorded as not run, never as success. An existing unqualified dependency target must fail configuration rather than be silently reused.

The schema correction adds a maintenance obligation and a second JSON DOM adds conversion cost. These are recorded choices, not hidden overhead. Revisit the correction when an upstream release includes an equivalent tested fix. Exact CI toolchain values and client dependencies are planned pins, not claims that those checks have run today.

Local verification can prepare CI configuration, but remote CI evidence stays pending until an authorized branch publication actually runs it. This planning commit does not publish a branch or claim a remote result.
