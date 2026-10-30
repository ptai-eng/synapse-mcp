# T16-T17 verifier and official-client checkpoint

Completed: 2026-10-29. Branch: `codex/product-protocol-foundation`. Commits remain local.

T16 and T17 are implemented, tested on Windows and Ubuntu WSL, and independently reviewed. The separate [T11-T15 audit](audit-t11-t15.md) is also complete. T18-T21, including the named-host walkthrough, CI and README, remain pending.

## Delivered behavior

- A private ESM verifier package pins Node 24.22.1, official MCP client 2.4.0 and Inspector 2.12.0 with a committed npm lockfile. npm ci --ignore-scripts and both public client imports passed. Local Node archives were checked against the release SHA-256 list fetched directly from nodejs.org; the global runtime was not replaced.
- Opt-in `SYNAPSE_MCP_BUILD_INTEROP_TESTS` checks prerequisites at configure time and registers absolute generator-resolved executable paths. Ordinary SDK builds do not require these tools.
- The official client permits only MCP 2025-11-25 in legacy negotiation mode. The harness records and asserts the actual initialize offer/reply and initialized notification, then discovers and exercises app.status, app://status and explain_status against the native reference process.
- Evidence has a fresh run ID, explicit status, timestamps and transcript. Failure replaces earlier success, and success is finalized after cleanup. Metadata records exact runtime/toolchain/STL, current checkout identity and binary SHA-256 separately. Binary build-source provenance is explicitly unknown; the runtime checkout is not presented as a binary build attestation.
- SDK requests retain 5s limits. The 15s overall budget reserves 1s for bounded child cleanup; the deadline tracks and reaps only owned native and metadata processes. Each of the four CTest cases has a 30s outer timeout. SDK forced cleanup is not graceful EOF proof; that remains a separate T18 requirement.

## Verification

| Platform | Official client and verifier regressions | Measured standard library |
|---|---|---|
| Native Windows/MSVC 19.44.35228.0 | 4/4 passed, 4.12s total | `_MSVC_STL_VERSION=143; _MSVC_STL_UPDATE=202503` |
| Ubuntu WSL/GCC 15.2.0 | 4/4 passed, 31.76s total across four sequential tests | `__GLIBCXX__=20260321` |

The four groups are client, evidence, provenance and cleanup. Negative controls cover a wrong revision, failing operation and missing executable. Durable regressions cover stale success, cleanup failure, changed runtime Git identity, Git failure, exact STL metadata and the deadline intersecting SDK close. The close-stage fixture is deterministic and verifies both reaping and child absence. The initial Ubuntu timing-based fixture failed and was replaced; it is not counted as passing evidence.

The original review requested four corrections: stale evidence, stale configure-time provenance, the watchdog/close race and inferred STL revision. Scoped rereviews approved all four with no open findings. The two extra verifier files contain durable regressions and the compiled STL probe; they add no server API.

## Evidence and reproduction

Local ignored artifacts: `build/execution/T16-report.md`, `T16-review.md`, `T17-report.md`, `T17-review.md`, `T17-fix-review.md`, `T17-fix2-review.md`, `T17-final-windows-*.log` and `T17-linux-*.log`. Actual transcripts are `build/t10-msvc/evidence/interop-client.json` and `build/t07-linux/evidence/interop-client.json`.

Prepend the matching checksum-verified Node directory under `build/tools/` to the command PATH, then:

```sh
npm --prefix tests/interop ci --ignore-scripts
cmake -S . -B <configured-build> -DSYNAPSE_MCP_BUILD_INTEROP_TESTS=ON
cmake --build <configured-build> --target native_status --parallel 2
ctest --test-dir <configured-build> --output-on-failure -R '^synapse\.M1\.interop\.'
```

For the existing multi-config MSVC tree, add `--config Debug` to build and `-C Debug` to CTest. Verified trees are `build/t10-msvc` and `build/t07-linux`. The older ignored `build/m1` had an incomplete SRELL checkout and is not used as success evidence.

Implementation commits: `d99b0ec` (T16) and `78027b9` (T17). Neither this SDK-client flow nor Inspector installation establishes the later named-host or CI acceptance gates.
