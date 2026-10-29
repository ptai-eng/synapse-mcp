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

## Resumed: T13-T15, 2026-10-28

- User authorized work through T15, then a report. T16-T21 and README remain deferred.
- T13 implementation reports focused MSVC24/24; task report/review still pending.
- T14 prompt regression fixture is prepared in `tests/prompts.cpp`; isolated RED confirms the missing `register_prompt` interface (`build/execution/T14-red-compile.log`). It is not yet integrated or complete.
- Ruling: Prepare the independent T15 reference state and test while the T13 resource implementation runs, without touching its server/CMake files; defer integration and completion until T14 is reviewed. The tool counter records status-tool invocations, while resource and prompt reads observe the same state, allowing matching snapshots without fixed metrics. Cost if wrong: bounded example/state refactoring once the prompt API is finalized. The T15 missing-header RED is recorded in `build/execution/T15-red-compile.log`.
- T13 reviewed and committed as `2c9eea6`; focused MSVC resource/lifecycle/policy selection24/24 passed. T14 source is being implemented and its RED fixture is in place.
- Ruling: Give consecutive invalid tool/prompt requests distinct test IDs. A response can be received before worker-side request retirement, so immediate ID reuse can spuriously trigger the deliberately enforced duplicate-in-flight rule. This changes test hygiene only; the original behavior remains covered. Cost if wrong: minor fixture maintenance.

- T14 reviewed and committed as `24f90e0`: MSVC prompt/resource/policy/lifecycle/ownership selection 25/25, prompt case 38 assertions. Review approves spec and quality.
- T15 source reviewed and committed as `39f8e86`; fake-clock case passed 14 assertions, Windows native stdio smoke passed. Full aggregate gates are running. Fixture ID correction committed separately as `e8b7dd5`.
- T13-T15 final verification: Windows/MSVC unfiltered CTest 104/104 and Ubuntu WSL/GCC 103/103; fresh downstream consumers and both real-process stdio probes passed on each platform. The mandatory official schema gate is included. All three task reviews approve source/spec and quality. See [T15 checkpoint](checkpoint-t15.md). T16-T21 and README remain pending.

## Resumed: T16-T17, 2026-10-28

- User authorized T16-T17 and a report; T18-T21 and README remain deferred.
- T16 RED found missing manifest, lock and runtime pin and global Node 24.15.0. Local Node 24.22.1 archive SHA-256 was independently matched to the Node release checksum over HTTPS. Exact client 2.4.0 and Inspector 2.12.0 installed via npm ci --ignore-scripts; both public client imports succeeded. Independent review approved spec and quality. Commit `d99b0ec`; evidence `build/execution/T16-report.md` and `T16-review.md`.
- T17 implementation in progress; exact negotiated revision, initialized notification, independent official client flow and negative controls remain execution gates.

## T11-T15 thorough audit, 2026-10-28

- User asked whether T11-T15 were complete and requested a thorough recheck. Prior task completion is historical; this audit includes full Windows/Ubuntu runs, cross-feature review and scoped repair before a renewed approval.
- Initial Windows baseline passed104/104 plus fresh consumer and raw stdio/native-state probes. Ubuntu baseline is still finishing dependency fixtures; runtime/schema/reference and consumer groups passed.
- Independent review found A1: direct control/pre-ready/queue-full emissions bypass the declared encoded-frame cap; A2: an oversized result from an already-cancelled operation can stop the whole session before the cancellation check; A3: resource URI syntax was not validated according to the selected MCP profile. These are open audit findings, not waived by the prior green tests.
- Ruling: extend the scoped audit repair to URI syntax checks and tests/resources.cpp alongside server.hpp/policy regressions, because the selected resource specification requires all resource URIs be validated. No resolver or filesystem/network access is introduced. Cost if wrong: bounded parser/refactoring work and additional URI compatibility testing.
- T17 remains uncommitted and awaiting its four independent-review corrections; no T17 approval is claimed. Its partial source changes are preserved while the requested T11-T15 audit takes priority.
- Audit fix round1: A1/A2/A3 addressed; standalone policy/resource9cases288assertions passed. Scoped rereview found two new P2s in the fix: gated fixture destruction order and initialize-success fallback leaving handshake state active.
- Audit fix round2: both P2s addressed; initialize RED reproduced2failed assertions, then16policy/resource/lifecycle cases418assertions passed. The URI syntax correction also required the old unknown-resource fixture to use app://missing instead of a malformed relative string, preserving the original -32002 assertion. Scoped rereview approves spec and quality without open findings. Source/tests saved in `682e4a9`.
- Fix1 full aggregates ran106/107 Windows and105/106 Ubuntu; the sole failure was the now-corrected old URI fixture. Final fix2 all-target builds, unfiltered suites, raw probes and10x focused repeats are still running; these intermediate results are not reported as final GREEN.

### Audit completion, 2026-10-29

- Final fix2 Windows unfiltered CTest 107/107 and Ubuntu WSL 106/106 passed; fresh downstream consumers and both actual-process stdio probes passed on each platform. The same 30 lifecycle/ownership/tool/policy/resource/prompt/reference cases each passed 10 consecutive executions on each platform (300 per platform). Schema official gate remains included. Source `682e4a9`, scoped review clean; [audit report](audit-t11-t15.md). T17 remains a separate active task.

### T17 completion, 2026-10-29

- Ruling: add tests/interop/harness.test.mjs and environment.cpp beyond T17's four named files, because stale-evidence/deadline regressions need a durable runner and standard-library revision must come from a compiled probe aligned with native_status. Cost if wrong: two verifier-only files to maintain, no server/API expansion.
- Ruling: distinguish the current runtime checkout from unknown binary build-source provenance and identify the executable separately by SHA-256. This removes the false configure-time identity claim without inventing an attestation. Cost if wrong: later release/CI work must add build-source attestation; this checkpoint does not claim it exists.
- Ruling: split verifier regressions into evidence/provenance/cleanup CTest cases with the existing 30s limit each, and trigger the fixture deadline only after SDK close clears its PID. The first Ubuntu timing-based test failed before that stage. The production 15s budget retains 1s for owned-child kill/reaping. Cost if wrong: a smaller active-work budget and three focused verifier groups instead of one timing-sensitive group.
- T17 initial review requested R1 stale evidence, R2 configure-time provenance, R3 watchdog/close cleanup and R4 exact STL metadata. Repairs included mutation/negative controls and durable regressions. The first repair rereview approved R1/R2/R4; final R3 rereview approved deterministic close-stage testing and actual native/metadata child reaping, no new findings.
- Final native Windows/MSVC official-client/verifier selection passed 4/4 in 4.12s; Ubuntu WSL/GCC passed 4/4 in 31.76s across four sequential cases (each under 30s). Actual transcripts assert 2025-11-25 and initialized plus tool/resource/prompt native-state flows. The exact STL probe reports MSVC 143/update 202503 and libstdc++ 20260321. Source/tests committed `78027b9`. No core source change followed the completed T11-T15 audit.
- T16/T17 complete, reviews clean; [checkpoint](checkpoint-t17.md). T18-T21, named-host walkthrough, CI and README remain pending. No push or publication occurred.

## Resumed: finish T18-T21, 2026-10-29

- User authorized completing T18-T21. Existing T01-T17 checkpoint remains valid; preserve .understand-anything and keep global MCP catalogs unchanged.
- T18 implemented and reviewed, commit `1aa2ecc`: raw-process Windows7/7 and Ubuntu7/7 post-review pass. Seven original negative controls plus idle/blocked early-success-exit controls fail correctly. Two review findings (fixture lifetime and pre-trigger exit oracle) were fixed and rereviewed; no core change.
- T19 prepared official action SHAs, exact GCC registry digest and Windows toolchain selection. Local workflow/modules implementation is authorized; remote branch publication is not yet authorized by this plan and remains a concrete final-step request before actual hosted runs. No local test substitutes for hosted CI evidence.
- Ruling: prepare the independent Inspector config and local host walkthrough while T19 workflow implementation proceeds, because T19 changes only verifier infrastructure and the native reference interface is stable. Keep T19/T20 qualification labels pending until their own actual evidence/reviews pass. Cost if wrong: repeat the host walkthrough if CI uncovers a runtime correction; no support claim is inferred from preparation.

### Scope adjustment and T18 handoff

- User is near usage limits and explicitly requested stopping after T18-T19. T20 preparation stopped without product edits, UI launch or catalog changes; T21 has not started. Inspector research resume notes remain ignored in build/execution/T20-paused.md.
- T18 checkpoint is recorded in [checkpoint-t18.md](checkpoint-t18.md); two P2 review findings are fixed and rereviewed clean. T19 workflow/modules are being locally verified; actual hosted matrix remains pending, and no push is authorized or performed.

### Stop for usage, 2026-10-29

- User requested status and stopping around T18-T19 because usage is nearly exhausted. T18 is complete, reviewed and committed as1aa2ecc. T19 implementation/local checks are ready, but its independent reviewer hit the account usage limit before returning a verdict. Therefore T19 review and hosted CI/sanitizer/failed-job evidence remain pending; T19 is NOT complete.
- Preserve the uncommitted T19 changes in .github/workflows/ci.yml and new cmake/RecordEnvironment.cmake / Sanitizers.cmake. Resume from build/execution/T19-report.md and T19-review.diff; obtain a fresh independent source review before any publication request. No push/dispatch occurred. T20-T21 remain deferred.

### T19 resumed after usage reset

- User reset usage and authorized continuing to T20 after T19 is complete and sound. T21/README remain deferred.
- T19 independent source review completed: implementation APPROVE, no actionable P1/P2 findings; actual hosted qualification remains pending. Minor limits: nested downstream consumer builds are functional-only rather than sanitizer-instrumented, and mutable runner utilities are version-recorded rather than fully image-attested.
- Reviewed implementation committed as `53190c8`. Remote branch does not yet exist. Requested explicit authorization to publish only codex/product-protocol-foundation and run verification Actions; no merge/master update/release is included. No remote mutation has occurred before authorization.

### Authorized hosted CI execution

- User explicitly authorized publishing codex/product-protocol-foundation and running verification Actions, without master merge or release. Pushed53190c8; mainrun40861978959. First dispatch controls40862013388(unit),40862057941(schema),40862064858(consumer),40862070566(interop) all failed during setup, so are NOT test-gate evidence.
- Actual Linux none/ASanUBSan/TSan runtime probes succeeded but configure failed when Git rejected container checkout ownership. Scoped exact-workspace Git trust correction reviewed APPROVE and pushed as1fa654b. New full matrix is triggered; actual results remain pending. No Docker startup on the user's machine and no host-global Git trust change.
- Corrected matrix run40862549576 at1fa654b passed Linux configure/runtime probes and reached functional tests. Re-dispatched intentional-failure controls at this corrected SHA:40863144391(unit),40863149665(schema),40863154296(consumer),40863160483(interop); conclusions must be checked against actual failing sentinel logs before credit.
- Initial Windows2022 job123407239864 in run40861978959 passed118/118 with MSVC19.44.35229.0, STL143/update202503, SDK10.0.26100.0, image20261026.347.1. This is a partial platform success, not a passed overall matrix.
- Hosted qualification review PASS at1fa654b: main40862549576 all4rows success (Windows118/118; Linux functional/ASanUBSan/TSan each117/117), four corrected dispatch controls fail exactlyone intendedsentinel with realcasespassing. Actualrecords/review retained in build/execution/T19-hosted-evidence.md and T19-hosted-review.md. [T19 checkpoint](checkpoint-t19.md) records exact versions/runlinks.
- Ruling: use the successful clean matrix and isolated negative-control runs at the same immutable source SHA as the complete gate evidence; do not repeat an identical full matrix merely to reorder completion timestamps. Fault injection modifies only disposable CTest build metadata. Cost if wrong: an additional unchanged workflow run, no source correction.
- T19 complete; user authorized proceeding to T20 after this gate. T20 generator/read-only host config implementation is active. T21/README remain deferred, and no master merge or release is included in existing authorization.
