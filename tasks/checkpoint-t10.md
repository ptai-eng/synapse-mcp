# T10 foundation checkpoint

Date: 2026-10-27. Branch: `codex/product-protocol-foundation`.
Foundation implementation: `d919cfe..dd35713`. Commits remain local.

**T01-T10 are implemented, locally verified and reviewed, including T05's mandatory schema gate.** This is the requested foundation checkpoint, not a completed M1 release. T11-T21 and README changes remain deferred.

## Delivered behavior

| Tasks | Result |
|---|---|
| T01-T03 | Pinned dependency correction and provenance checks, downstream C++23/header-only build contract, typed errors/handlers/options |
| T04 | Incoming JSON-RPC validation, notification handling and lossless string/signed/unsigned request IDs |
| T05 | Offline schema adapter, Unicode matching and dynamic resource resolution; mandatory suite qualified locally |
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
| Windows 11, native MSVC 19.44.35228.0, Windows SDK 10.0.26100.0 | 95/95 unfiltered CTest cases pass |
| Ubuntu under WSL, GCC 15.2.0 | 94/94 unfiltered CTest cases pass |
| T10 lifecycle/ownership executable, native MSVC | 18 cases / 502 assertions pass |
| Schema adapter executable, Windows GNU | 7 cases / 2415 assertions pass |
| Pinned official mandatory JSON Schema 2020-12 suite | 1302/1302 cases pass; zero skips |
| Fresh downstream consumer, Windows and Linux | Configure, build and execution pass, including Unicode schema validation |
| CMake minimum-version consumer and parent fixtures | CMake 3.20.5 configure/build/execution and rejection cases pass |
| Raw child-process wire probe, Windows and Linux | Revision counteroffer, resource read, ping, malformed/numeric-overflow recovery, notification silence, large/string IDs and graceful EOF pass |

Both full CTest totals include the unchanged official schema gate. All 46 mandatory suite files, 384 groups and 1302 cases passed without skips. The previous seven failures in dynamic-reference scope and Unicode property escapes are corrected. This mandatory-suite result does not establish optional-format behavior or every possible regular expression.

The raw probe launches the actual example and requires exit code 0 within two seconds after closing stdin; forced cleanup is failure. Both executions passed without forced cleanup or stderr output. This is a smoke check, not official-client interoperability or the complete T18 process suite.

T10 review found and fixed interrupted-start joins and cross-instance/nested callback wait tracking (`56ef7ff`). Ordered test-fixture cleanup was fixed in `2138f90`; scoped rereview approved compliance and quality with no remaining findings. The MSVC adapter build also required the narrowly guarded `/bigobj` correction (`8f4f539`).

The independent T01-T10 audit found and closed three additional issues: numeric-overflow recovery and test callback lifetime (`4286da6`), dependency-tree provenance (`ff4fb90`), and schema dialect identity/CMake-floor fixture corrections (`dd35713`). Follow-up reviews approved the scoped fixes. [ADR-004](../docs/decisions/0004-schema-qualification-corrections.md) records the pinned Unicode engine, seven-file version-locked backend correction and migration checks.

## Reproduce in the configured build trees

Windows (PowerShell):

```powershell
cmake --build build/t10-msvc --config Debug --parallel 3
ctest --test-dir build/t10-msvc -C Debug --output-on-failure
```

Linux (inside Ubuntu WSL, from the repository):

```sh
cmake --build build/t07-linux --parallel 3
ctest --test-dir build/t07-linux --output-on-failure
```

These commands reuse configured local trees, not a fresh-checkout installation guide. Docker was not used for final Linux verification after the user closed it.

Detailed local evidence is retained under ignored `build/execution/`: `T01-T10-final-{windows,linux}-{build,test}.log`, `T01-T10-final-{wire,consumer}-{windows,linux}.json`, `T01-T10-independent-audit.md`, `T09-audit-fix-review.md` and `T05-review-fix-review.md`. These logs are not committed artifacts.

## Review boundary and next work

The [execution ledger](execution.md) records the implementation rulings and their rework risks. The scope decisions included minimal resources for lifecycle verification and a version-locked dependency correction rather than suppressing schema failures. Compiler fixes change build compatibility; ADR-004 records the regex and resource-identity semantics that changed.

Continue the remaining tool/policy/resource/prompt slices, independent interoperability/process checks and CI/host evidence only after this checkpoint. README remains T21. No push, PR, release or global MCP configuration was performed.
