# T13-T15 methods and native reference checkpoint

Date: 2026-10-28. Branch: `codex/product-protocol-foundation`. All commits remain local. The subsequent [thorough audit](audit-t11-t15.md) records additional corrections and final verification on 2026-10-29.

T13 completes static text resources, T14 adds static text prompts, and T15 adds a native application demonstrating a shared real state. Independent scoped reviews approved their source and quality. This checkpoint does not claim official-client, named-host or CI interoperability; those are T16-T20. README work remains T21.

## Behavior

- Resources expose static list/read and an empty template list. Missing, invalid and unknown URIs and callback failures take the documented error paths. Resource text is subject to the existing worker and output policy. Subscriptions and list changes are not advertised.
- Prompts expose static list/get with required and optional string arguments, reject unknown or malformed arguments, and return user/assistant text messages. Returned descriptions and message text pass output filtering; handler failures are sanitized. Completion and multimodal support are not advertised.
- `native_status` exposes `app.status`, `app://status` and `explain_status`. Its shared application state measures elapsed steady-clock time and counts invocations of the status tool. Resource and prompt reads observe that state without incrementing the counter; the output states its counter scope. A fake clock verifies identical snapshots at the same instant.

## Evidence

- T13 focused MSVC selection: 24/24 tests; resource case: 43 assertions.
- T14 focused MSVC selection: 25/25 tests; prompt case: 38 assertions.
- T15 fake-clock reference case: 14 assertions. Independent source reviews: `build/execution/T13-review.md`, `T14-review.md`, and `T15-review.md` (local ignored artifacts).
- Windows native MSVC full unfiltered CTest: 104/104; fresh downstream consumer execution passed; actual stdio probes passed for the existing example and native status, including graceful EOF.
- Ubuntu WSL / GCC 15.2 full unfiltered CTest: 103/103; fresh downstream consumer execution passed; actual stdio probes for both examples passed, including graceful EOF.

Real-process smoke checks exercise local transport and state; they do not substitute for the official-client and named-host verification planned later. Logs and probe outputs are retained locally under ignored `build/execution/`.

## Re-run

Windows: `cmake --build build/t10-msvc --config Debug --parallel 2` then `ctest --test-dir build/t10-msvc -C Debug --output-on-failure`.

Ubuntu WSL: `cmake --build build/t07-linux --parallel 2` then `ctest --test-dir build/t07-linux --output-on-failure`.

Implementation commits: `2c9eea6` (resources), `24f90e0` (prompts), `39f8e86` (native status), and `e8b7dd5` (test request IDs).
