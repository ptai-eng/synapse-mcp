# T19 hosted CI checkpoint

Completed: 2026-10-29. Verified source: `1fa654be73d582e9ae4778da0d757e9112a6bac1`. Independent source and hosted-evidence reviews approve T19.

The [main hosted run](https://github.com/ptai-eng/synapse-mcp/actions/runs/40862549576) passed all four matrix rows:

| Configuration | Result | Actual compiler / standard library |
|---|---|---|
| Windows Server 2022, VS toolset 14.44, SDK 10.0.26100.0 | 118/118 | MSVC 19.44.35229.0; STL 143/update 202503 |
| Linux functional, pinned GCC container | 117/117 | GCC 14.4.0; libstdc++ 20260626 |
| Linux ASan+UBSan, separate build | 117/117 | GCC 14.4.0; runtime probe, instrumented build and tests passed |
| Linux TSan, separate build | 117/117 | GCC 14.4.0; runtime probe, instrumented build and tests passed |

Each Linux row ran 77 unit, 26 schema, 3 consumer, 7 raw-process and 4 official-client/verifier cases inside the same pinned container. Nested downstream consumer builds verify functional integration; they do not inherit sanitizer instrumentation. The SDK/unit/process targets are instrumented in sanitizer rows. No sanitizer was skipped or reported successful without execution.

The container is `gcc:14.4.0@sha256:435c3cbc2ad1c25fbc1684c367808ff0ca2035a1bed56734a40165ab5cd7e3bd`. Node 24.22.1, official client 2.4.0 and Inspector 2.12.0 remain exact pins. All rows recorded clean tracked worktrees. Actions runner 2.338.0 setup logs identify Ubuntu 24.04 image 20261025.353.1 and Windows 2022 image 20261025.346.1. The Windows exported ImageVersion separately reports 20261026.347.1; raw evidence preserves both provider fields rather than asserting they are identical.

## Failure propagation

Four hosted controls at the same source revision each failed with exactly one deliberate sentinel while the real cases passed:

| Family | Hosted failed run | Observed result |
|---|---|---|
| Unit | [40863144391](https://github.com/ptai-eng/synapse-mcp/actions/runs/40863144391) | 77 real cases pass; unit sentinel fails |
| Schema | [40863149665](https://github.com/ptai-eng/synapse-mcp/actions/runs/40863149665) | 26 real cases pass; schema sentinel fails |
| Consumer | [40863154296](https://github.com/ptai-eng/synapse-mcp/actions/runs/40863154296) | 3 real cases pass; consumer sentinel fails |
| Interop | [40863160483](https://github.com/ptai-eng/synapse-mcp/actions/runs/40863160483) | 4 real cases pass; interop sentinel fails |

Fault injection changes only disposable generated CTest metadata. The successful main run and isolated negative controls use the same immutable source SHA; these failures did not modify source or the successful matrix's build trees.

## Corrections and evidence

The first hosted Linux attempt exposed Git's checkout-ownership guard in the container. A reviewed correction trusts only the explicit GitHub workspace inside that ephemeral container. The initial setup-failed controls are excluded from failure-propagation evidence. The corrected matrix and controls above establish T19; local-only checks are not substituted for them.

The workflow triggers master/codex/** pushes, master pull requests and manual dispatch, with full action SHA pins and read-only token permissions. It retains allowlisted logs, environment metadata and transcripts even on failure. TSan uses a child-scoped ASLR workaround in its CI container; no user-machine Docker or host sysctl change was needed.

Ignored local primary evidence: `build/execution/T19-hosted-evidence.md`, `T19-hosted-review.md`, `T19-matrix-status.json`, downloaded `ci-1fa-*` / `ci-probe-*` artifacts and `T19-1fa-*.log`. Source reviews are `T19-review.md` and `T19-safe-directory-review.md`. Implementation commits: `53190c8` and `1fa654b`.

The user authorized the feature-branch publication and verification runs. Master was not merged and no release was created. T20 is the next named-host gate; T21/README remains deferred.
