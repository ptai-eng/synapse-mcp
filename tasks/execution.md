# M1 execution ledger

Plan: [plan.md](plan.md). Checklist: [todo.md](todo.md).

- Execution started from `d919cfe` on `codex/product-protocol-foundation`.
- User authorized implementation of M1. Existing `.understand-anything/` artifacts remain outside product commits.
- User subsequently chose a T10 checkpoint: execute T01-T10, then stop for their review; leave T11-T21 pending.
- CodeGraph is absent and codebase-memory has no Synapse index; source reads are permitted.
- T01 implementation started. T02 prepares its independent failing consumer fixture; its production changes wait for T01 review.
- Ruling: Prepare T03's isolated public headers and failing contract tests alongside build qualification, but defer CMake integration, completion and review until T02 passes. These files do not change existing runtime paths; the cost if the prerequisite changes is confined contract-fixture rework.

## Progress

| Task | State | Evidence |
|---|---|---|
| T01 | Complete; reviewed | `73506f0`; 12/12; `build/execution/T01-review.md` |
| T02 | Complete; reviewed | `eb66bb3`; 18/18; `build/execution/T02-review.md` |
| T03 | Implemented; review in progress | `b830540`; 5/5 plus standalone headers |
| T04 | In progress | `build/execution/T04-report.md` pending |
| T05 | In progress; qualification blocker known | `build/execution/T05-preparation-report.md` |
| T06-T10 | Pending | Next independent runtime tasks |
| T11-T21 | Deferred by user | Review after T10 first |

## Reviews and decisions

T01 and T02 passed scoped compliance and quality review with no blocking findings. T02 has a nonblocking single-config build-type forwarding caveat, to carry into final checkpoint verification.

- Ruling: Propagate `-Wa,-mbig-obj` only for GNU on Windows through the qualified jsoncons interface. The actual schema translation unit exceeded MinGW's object-section limit; the cost if wrong is an unnecessarily larger object format on that compiler, not a change to schema behavior or other toolchains.
- Ruling: Extend ADR-003's build-tree correction to the three NUL-truncating `allowed_properties.find(prop.key().c_str())` lookups demonstrated by T01. Preserve full-length `prop.key()` instead, retain the failing evidence and add regression coverage for each path. The mandatory NUL-key case cannot pass with the pattern correction alone; the cost if wrong is a bounded, removable dependency patch and qualification rework. No test is skipped and no tracked vendor header is edited.
- Ruling: Include the two equivalent `properties_.find(prop.key().c_str())` defects demonstrated by prefix-collision regressions. Audit the same frozen file once for this class of truncation, preserve full key identity, and test both validation and walk paths. Leaving these defects would misclassify a NUL-containing key as its prefix; the cost if wrong remains a reversible, version-locked patch.
- Ruling: The one-pass audit found two equivalent evaluated-property lookups. Include them in ADR-003 with false/schema-valued unevaluated-property regressions. The final bounded correction is one string-range change plus seven key lookups; diagnostic rendering stays unchanged. Leaving the lookups would corrupt evaluated-key identity; the cost if wrong is removable patch maintenance.
- Ruling: Prepare T02 production CMake integration while T01 finishes its independent correction tests, but defer T02 completion/commit until T01 review passes. Its dependency function/target interface is stable and file ownership is disjoint; the cost if wrong is a small CMake integration rework rather than an unqualified schema claim.
- Qualification finding: The unpatched pinned backend probe ran all 1,302 non-optional 2020-12 cases (46 files, 384 groups), with 1,295 passing and seven failing: one dynamic-reference scope case and six Unicode property-escape cases. Evidence: `build/execution/T05-preparation-suite.log`. The amended T01 source must be tested again through the adapter; this preliminary run is not its final result.
- Ruling: Do not expand this checkpoint into a replacement regex/backend project. Implement and expose the T05 qualification gate honestly, retain genuine failures, and leave T05 qualification/T11/full-2020-12 claims blocked if they persist. Independent T06-T09 work and T10 lifecycle integration may proceed for user review, but no tool migration or milestone-complete claim follows. This preserves useful runtime work without weakening the mandatory schema gate; the cost if wrong is later adapter integration rework.
- Ruling: A preloaded custom metaschema may describe a supported 2020-12-derived dialect. Resolve it offline and inspect its required vocabularies instead of rejecting solely because its URI differs from the standard metaschema URI. Unsupported required vocabularies still fail. Strict URI equality would incorrectly reject five mandatory vocabulary tests; the cost if wrong is a bounded dialect-resolution fix.

- Ruling: Parallelize independent source/test work with disjoint file ownership; serialize edits to tests/CMakeLists.txt and git staging/commits. Use per-task test targets so unfinished independent files do not enter another task's build. This follows the active parallel-execution instruction while avoiding shared-file conflicts; the cost if wrong is limited build integration rework.

## User stop checkpoint

User explicitly requested stopping now to preserve remaining usage. No further implementation or testing is authorized until resumed.

- T01: complete and reviewed, commit 73506f0; 12/12 qualification regressions.
- T02: complete and reviewed, commit eb66bb3; fresh consumer and accumulated 18/18 checks.
- T03: complete and reviewed, commit b830540; 5/5 targeted checks plus standalone headers.
- T04: implementation committed (4c2e10c, 02becce); 6 protocol cases / 364 assertions; accumulated targeted 11/11. Fresh task review remains pending.
- T05: RED fixture committed a066843. Adapter header remains untracked. Preliminary backend suite has 7/1302 mandatory failures; adapter first run also exposed four URI-resolution regressions, whose fix was rebuilding when interrupted. Final adapter result and review are NOT established. Do not mark qualified or migrate tools.
- T06: implementation committed (a848d68, a1e56ff); 9/9 executor checks reported. Report exists at build/execution/T06-report.md. Fresh review remains pending.
- T07: common interface/tests/CMake committed 51da1ea. POSIX implementation header remains untracked, not Linux-verified; report not finalized.
- T08-T10: not started. T11-T21 remain deferred.
- MSVC 14.44.35207 / Windows SDK 10.0.26100.0 configured successfully; serialization, contract and protocol targets built. MSVC CTest was not run.
- Linux helper container synapse-m1-linux-check was stopped. Tool installation was incomplete; no Linux test pass is claimed. The exact GCC CI image prefetch was canceled earlier.
- Shared build/m1 had overlapping Ninja builds and recovery warnings. On resumption serialize build commands or use separate per-task build trees; use committed test targets and reconfigure before checkpoint validation.
- Product commits stay local on codex/product-protocol-foundation. No push/release/host setup occurred. Preserve all untracked headers and .understand-anything artifacts.

## Resumed 2026-10-26

- User reauthorized continuing through T10, then stopping for their review. Foundation work takes priority; README remains for the final documentation stage.
- The previous stop checkpoint above is historical and its pause is now lifted.
- T04 scoped review passed with no blocking findings (build/execution/T04-review.md).
- T06 scoped review found a capture-destruction-under-lock path when list-node allocation fails in enqueue. Fix round 1 is in progress with a targeted allocation-failure/reentry regression.
- T05 resumes its existing adapter implementation and real failing qualification gate; no backend/regex redesign or tool migration is authorized in this checkpoint.
- T08 Windows I/O implementation started against T07's committed common interface. The user closed Docker; Windows verification proceeds independently. Linux verification remains pending unless another available Linux environment can run it.
- Separate build trees avoid the previous Ninja recovery warnings: T05 uses build/m1; T06 fix uses build/m1-msvc; T08 uses build/t08-msvc. Shared CMake edits/commits remain serialized.
