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
| T01 | Complete; reviewed | `73506f0`; MSVC `/bigobj` correction `8f4f539`; regression gates pass |
| T02 | Complete; reviewed | `eb66bb3`; fresh consumer verified |
| T03 | Complete; reviewed | `b830540`; contract and standalone headers |
| T04 | Complete; reviewed | `02becce`; umbrella macro integration fix `6c85b53` |
| T05 | Complete; locally qualified and reviewed | `23310aa`, `ff4fb90`, `dd35713`; official1302/1302, zero skips; final Windows95/95 and WSL94/94 unfiltered |
| T06 | Complete; fix reviewed | `a1e56ff` + `84423ae`; allocation-failure cleanup fixed |
| T07 | Complete; reviewed | `3b29e12`; WSL GCC15.2 POSIX10/10; 36/36 selected regression |
| T08 | Complete; reviewed | `94f58ac`; native MSVC9cases/978assertions; 31/31 selected regression |
| T09 | Complete; reviewed | `8fd8d25`; Windows16/16, Linux14/14 |
| T10 | Complete; reviewed | `bcfcb0e`, `56ef7ff`, `2138f90`; full current Windows95/95 and WSL94/94, including schema gate |
| T11 | Complete; reviewed | `fce5e1b`; schema-validated tool registration/list/call |
| T12 | Complete; fix reviewed | `8a39fc1`; policy and final-frame handling |
| T13-T21 | Deferred by user | Stop after T12; README last |
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

### T10 integration boundary

Ruling: Prepare T10 lifecycle source/tests in parallel with T09's stable transport-interface implementation, but defer T10 completion until T09 passes its verification/review gate. Files and build trees are disjoint; the cost if wrong is limited integration rework.

Ruling: Exercise application-worker ownership through a minimal typed static resource list/read path and register_resource in T10. Tools/prompts migration stays deferred to T11-T14, and tools must not be advertised while the T05 gate fails. This uses a real public request/handler boundary without introducing a test-only public dispatch API. Full resource/profile qualification still belongs to T13; T10 is not a complete M1 claim. The cost if wrong is bounded route refactoring when the later slices land.


### Final checkpoint verification

- Ruling: MSVC C1128 in the actual full adapter translation unit requires `/bigobj`; propagate it only for Windows/MSVC through qualified jsoncons, keeping the GNU Windows option unchanged. Implemented in `8f4f539` and reviewed as correct scope. Cost if wrong: a harmless compiler object-format option on this compiler, not schema semantics.
- Ruling: Public umbrella integration exposed Windows min/max macros in decoder numeric_limits calls; three parenthesized calls in `6c85b53` preserve caller macros. The umbrella include now has a compile regression in ownership tests. Cost if wrong: a small portable syntax correction, no protocol change.
- T05 now ran through the actual adapter on MSVC and WSL GCC15.2: exactly seven mandatory failures each, all 1302 cases executed. Detailed gate exits are 8. No case is skipped or marked expected-pass. Default CTest therefore remains failing until T05 is resolved.
- Initial whole-checkpoint Linux run passed 77/77 tests excluding only the separately executed failing official gate. Final runtime fix adds three cases; rerun once fix round2 is committed. Windows full build first exposed C1128; corrected schema target now builds, final full checkpoint still pending.
- T10 review P2s (interrupted-start cleanup, per-instance nested callback waits) fixed by `56ef7ff`; ASTRA re-review approves production fixes and compiler guard. New test-only assertion-unwind lifetime problem is being fixed in round2. Re-review only that cleanup delta afterward.
- Remaining before user handoff: round2 commit/re-review; final full Windows+WSL build/CTest excluding only explicit official gate; raw Python wire smoke against both examples; persist checkpoint summary/status and stop before T11. No README edit or push.

### T10 handoff completed, 2026-10-26

- Task T10: fix round 1/5 (2 production findings addressed; new test-only lifetime finding open; commit `56ef7ff`).
- Task T10: fix round 2/5 (ordered assertion-unwind cleanup addressed, 0 open; commit `2138f90`). Scoped rereview approves spec compliance and quality in `build/execution/T10-fix2-review.md`.
- Task T10: runtime complete (implementation/review fixes `bcfcb0e..2138f90`, task review clean), under the recorded exception for T05. No full-M1 qualification claim.
- Final all-target builds, including both examples, succeeded on native Windows/MSVC 19.44 and Ubuntu WSL/GCC15.2. Selected CTest passed Windows **81/81** and Linux **80/80**. The only excluded test was the separately executed, known-failing official schema gate; full default CTest remains failing.
- Raw Python child-process probes passed on both actual examples: `2025-11-25` counteroffer, initialized flow, resources/list/read, ping, malformed-envelope/parse recovery, notification silence, uint64/string IDs, EOF exit code0 within2s without forced cleanup. Saved wire transcripts have empty stderr. This is smoke evidence, not official-client/host or full T18 verification.
- T05 remains BLOCKED: 1295/1302 mandatory cases pass on both native MSVC and WSL; seven genuine failures, zero skips. Source-quality review is approved, qualification is not. T11 must wait.
- [Checkpoint report](checkpoint-t10.md) is the review entrypoint. ADR status labels now match the implemented runtime and failed schema gate. README remains untouched, changes stay local, `.understand-anything/` is preserved, and Docker was not restarted.
- All requested work stops here for user review. T11-T21 remain unchecked.

## Resumed: complete and audit T01-T10, 2026-10-26

- User explicitly authorized resolving T05 and repairing/reviewing the complete T01-T10 foundation. The preceding review stop is lifted; T11-T21 and README remain deferred.
- The current schema gate was rerun before fixes: exactly seven failures, 1302 cases, zero skips, CTest exit8 (`build/execution/T05-resume-red.log`).
- Ruling: [ADR-004](../docs/decisions/0004-schema-qualification-corrections.md) supersedes the earlier NUL-only boundary to add canonical resource associations and a pinned header-only Unicode regex engine inside normal backend evaluation. Root-cause probes establish both defects; no skip list or weakened claim is allowed. Cost if wrong: additional patch/dependency maintenance and bounded integration rework, caught by the unchanged mandatory gate and focused regressions.
- Independent whole-foundation review is running against `d919cfe..bbf7ed7`; new fixes receive their own scoped review and final accumulated validation before this checkpoint is declared complete.
- Independent review completed with three P2 findings: parser numeric-overflow recovery (F1), dirty dependency content qualification (F2) and T09 fixture capture lifetime on assertion unwind (F3). F2 is assigned to the T05 dependency correction; F1/F3 form one scoped runtime/test repair batch.
- Ruling: Run the T05 dependency repair and the disjoint two-file T09 repair concurrently, but give T09 a direct standalone Catch2 build so it never regenerates CMake while the dependency files change. Shared CMake edits remain exclusive to T05; all Git staging/commits are serialized by the controller. Cost if wrong: confined integration/retest work, caught by final all-target builds and scoped review.

### Continuation 2026-10-27

- The prior agent turns were interrupted by usage limits before reports, review or commits were complete. Working source edits are preserved; HEAD remains `bbf7ed7`. No completion is implied by those edits.
- Initial T05 focused corrections passed; the first complete adapter run exposed four URN r-/q-component registration failures and one new fragment-resource lifetime fixture failure. The mandatory suite still ran all 1302 cases with zero skips. These regressions must be fixed before qualification.
- T05 and the T09 runtime/test repair have resumed from existing artifacts. Final native MSVC/WSL all-target builds, unfiltered CTest, actual consumer execution, updated raw-wire probes and scoped review are still pending.
- T09 F1/F3 repaired in `4286da6`: RED reproduced numeric overflow; standalone Linux GREEN passed 48 focused assertions in2 cases and 136 assertions in15 stdio cases. Scoped review and native Windows aggregate remain pending.
- Ruling: Extend the T05 adapter repair to preserve query-bearing resource identity and canonical anchor lookup through retrieval aliases, as specified in the ADR-004 qualification amendment. The first full run establishes the URI bug, and the alias lifetime regression establishes the missing canonical lookup. Cost if wrong: bounded adapter/association rework, checked by the unfiltered suite and explicit query/alias/nested-resource regressions.
- T05 implementation committed `ff4fb90`: standalone19/19; schema6 cases/2372 assertions; unchanged mandatory1302/1302,zero skips; fresh consumer executes successfully. Scoped review is in progress.
- Initial final native MSVC all-target build and unfiltered CTest pass94/94, including actual consumer runtime and raw-wire numeric-overflow recovery/EOF. Linux run remains in progress. These results do not waive subsequent review findings.
- Scoped review found a CMake-floor fixture defect: `FetchContent_SetPopulated` is unavailable at the promised3.20 floor. Actual CMake3.20.5 reproduced the wrong-error failure (`build/execution/T05-cmake320-red.log`). A supported local population fixture must replace it and pass minimum-version verification before handoff.
- T05 scoped review requests exactly two P2 fixes: CMake-floor fixture and query-bearing custom dialect lookup in backend `json_schema_factory.hpp`; F2 provenance is addressed. ADR-004 now explicitly authorizes the seventh backend file with exact migration qualification. Existing passing1302 suite is genuine but does not waive the additional regression.
- Initial WSL full run passed92/93; only the expanded negative-state provenance test timed out at120.10s. All runtime and mandatory-schema cases passed. The controller initially inferred30s from the older fixture; the actual log and current CTest properties establish120s. Ruling: preserve all provenance assertions, narrow repeated fixture restores to changed header paths to avoid rescanning the whole upstream tree, and measure any necessary bounded budget adjustment; retain runtime/EOF deadlines. Cost if wrong: longer failure detection for a build fixture, not weaker protocol behavior.
- T05 review fixes committed as `dd35713`: the CMake3.20-compatible populated fixture and query-preserving custom-dialect lookup are implemented. Focused run passed1302/1302 mandatory cases (zero skips;2415 assertions/7 cases), CMake3.20/current parent fixtures pass, and the optimized WSL provenance fixture passed in89.96s within its unchanged120s limit. Scoped rereview approves spec and quality with no open findings (`build/execution/T05-review-fix-review.md`).
- Final Windows/MSVC unfiltered CTest passed95/95; final Ubuntu WSL/GCC15.2 unfiltered CTest passed94/94. Fresh downstream consumers and raw-wire numeric-overflow/EOF probes passed on both. A CMake3.20.5 fresh consumer also configured, built and ran successfully. Full logs and transcripts are under `build/execution/T01-T10-final-*`.
- T01-T10 foundation completion: independent baseline review found F1/F2/F3; `4286da6`, `ff4fb90` and `dd35713` address them and the additional schema-review findings. Scoped rereviews approve spec and quality with no open findings. T05's mandatory schema suite runs1302/1302 cases with zero skips. README and T11-T21 remain pending; this is not a full M1 or host-compatibility claim.

## Resumed: T11-T12, 2026-10-28

- User authorized T11-T12, followed by a report; leave T13-T21 and README pending.
- T11 RED observed missing tool registration; implementation and targeted Windows schema/lifecycle tests are underway. T12 tests and policy module are prepared separately.
- Ruling: Expose the stdio transport's effective encoded-frame capacity to the server for T12 output checks. The server otherwise cannot reject oversized handler output as a tool failure before transport admission; the accessor is read-only and retains the existing limits. Cost if wrong: a small public accessor and transport/server integration rework, covered by encoded-output and transport regressions.
- T11 complete: `fce5e1b`, independent spec and quality review approved (`build/execution/T11-review.md`); static registration, schema validation, list/call and result/error channels.
- T12 implemented as `8a39fc1`: initial RED failed 3 policy cases; targeted MSVC GREEN passed 23/23. Review found an oversized fallback response with a long ID; a new focused RED reproduced it, then GREEN passed all5 policy cases/68 assertions. Scoped rereview approved (`build/execution/T12-fix-review.md`).
- Final MSVC selection passed48/48 and equivalent Ubuntu WSL/GCC15.2 selection passed46/46, including tool, policy, lifecycle, ownership, schema (including the official gate) and stdio. A raw Windows GNU child-process probe also passed actual tool listing/call, schema rejection, access denial, output filtering, ping and graceful EOF. This is not official-client or named-host verification. See [T12 checkpoint](checkpoint-t12.md) for commands and scope.
