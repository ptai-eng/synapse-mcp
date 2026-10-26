# T10 foundation checkpoint

Date: 2026-10-26. Branch: `codex/product-protocol-foundation`.
Runtime implementation: `d919cfe..2138f90`. Commits remain local.

**Runtime through T10 is implemented, tested and task-reviewed. T05 schema qualification is blocked.** This is the user-requested review stop, not a completed M1 release. T11-T21 and README changes remain deferred.

## Delivered behavior

| Tasks | Result |
|---|---|
| T01-T03 | Pinned dependency correction and provenance checks, downstream C++23/header-only build contract, typed errors/handlers/options |
| T04 | Incoming JSON-RPC validation, notification handling and lossless string/signed/unsigned request IDs |
| T05 | Offline schema adapter implemented and source-reviewed; mandatory qualification fails as recorded below |
| T06 | One owned application worker, bounded pending queue, cancellation, stop/join and timed waits |
| T07-T09 | Interruptible POSIX/Windows pipe I/O, bounded newline framing/output and owned reader/writer shutdown |
| T10 | MCP `2025-11-25` negotiation and initialization ordering, ping, in-flight IDs/cancellation, frozen registration and server lifetime ownership |

T10 uses a minimal typed static resource list/read path to exercise real application handlers. `examples/simple_server.cpp` serves `app://greeting`. The server does not advertise tools or prompts at this checkpoint; those slices and the complete resource/output policy remain later tasks.

Defaults include one active callback, 16 pending calls, a 1 MiB input frame, depth 64, 4 MiB outstanding encoded output and a 250 ms output flush budget. No SDK worker is detached. Captured application state and the borrowed transport must outlive the server and joined work. Arbitrary noncooperative callbacks can keep `wait_for` false and delay destruction; cancellation does not forcibly terminate application code.

The experimental server reverse-request API (`server::send_request`) and outgoing promises were removed. Existing integrations must adapt to the typed lifecycle/handler contracts. The separate client example compiles but remains experimental.

## Verification

Final builds include both examples and all test targets. These are local platform checks; the exact supported CI matrix, sanitizers, official MCP client and named host verification remain pending in T16-T20.

| Environment / check | Actual result |
|---|---|
| Windows 11, native MSVC 19.44.35228.0, Windows SDK 10.0.26100.0 | 81/81 selected CTest cases pass |
| Ubuntu 26.04 under WSL, GCC 15.2.0 | 80/80 selected CTest cases pass |
| T10 lifecycle/ownership executable, native MSVC | 18 cases / 502 assertions pass |
| Local schema adapter regressions | 295/295 assertions pass |
| Pinned official mandatory JSON Schema 2020-12 suite, MSVC and WSL | 1295/1302 cases pass; 7 fail; zero skips |
| Raw child-process wire probe, Windows and Linux | Revision counteroffer, initialization, resource read, ping, malformed-message recovery, notification silence, large/string IDs and graceful EOF pass |

The selected CTest totals exclude exactly `synapse.M1.schema.official_draft2020_12`, which was run separately and fails normally (CTest exit 8). Default full CTest is therefore **not green**. All 46 mandatory suite files, 384 groups and 1302 cases were exercised: one dynamic-reference scope case and six Unicode property-escape cases fail in the pinned backend. No failure was disabled or inverted. T11/tool migration and full 2020-12 support claims remain blocked.

The raw probe launches the actual example and requires exit code 0 within two seconds after closing stdin; forced cleanup is failure. Both executions passed without forced cleanup or stderr output. This is a smoke check, not official-client interoperability or the complete T18 process suite.

T10 review found and fixed interrupted-start joins and cross-instance/nested callback wait tracking (`56ef7ff`). Ordered test-fixture cleanup was fixed in `2138f90`; scoped rereview approved compliance and quality with no remaining findings. The MSVC adapter build also required the narrowly guarded `/bigobj` correction (`8f4f539`).

## Reproduce in the configured build trees

Windows (PowerShell):

```powershell
cmake --build build/t10-msvc --config Debug --parallel 3
ctest --test-dir build/t10-msvc -C Debug --output-on-failure -E '^synapse\.M1\.schema\.official_draft2020_12$'
# Separate mandatory gate: currently fails with the seven cases above.
ctest --test-dir build/t10-msvc -C Debug --output-on-failure -R '^synapse\.M1\.schema\.official_draft2020_12$'
```

Linux (inside Ubuntu WSL, from the repository):

```sh
cmake --build build/t07-linux --parallel 3
ctest --test-dir build/t07-linux --output-on-failure -E '^synapse\.M1\.schema\.official_draft2020_12$'
# Separate mandatory gate: currently fails with the seven cases above.
ctest --test-dir build/t07-linux --output-on-failure -R '^synapse\.M1\.schema\.official_draft2020_12$'
```

These commands reuse configured local trees, not a fresh-checkout installation guide. Docker was not used for final Linux verification after the user closed it.

Detailed local evidence is retained under ignored `build/execution/`: `T10-checkpoint-{windows,linux}-{build,test}.log`, `T10-wire-{windows,linux}.json`, `T10-fix2-{report,review}.md`, `T05-msvc-gate.log` and `T05-linux-test.log`. These logs are not committed artifacts.

## Review boundary and next work

The [execution ledger](execution.md) records every implementation ruling and its rework risk. The principal scope decisions were to retain the failing schema gate, continue independent runtime work without tools, and use minimal resources for lifecycle verification. The dependency correction stays version-locked and limited to the documented string/key fixes; compiler fixes change build compatibility, not schema semantics.

Review this checkpoint before further implementation. Resolve T05's backend/regex qualification decision before T11. Continue the remaining tool/policy/resource/prompt slices, independent interoperability/process checks and CI/host evidence afterward. README remains T21. No push, PR, release or global MCP configuration was performed.
