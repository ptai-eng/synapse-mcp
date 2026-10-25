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
| T01 | In progress | `build/execution/T01-report.md` |
| T02 | RED preparation | `build/execution/T02-red-report.md` |
| T03 | Isolated preparation | `build/execution/T03-report.md`; not complete |
| T04-T21 | Pending | No implementation claims |

## Reviews and decisions

No task has passed its review gate yet. Append review outcomes and any implementation rulings here as work proceeds.

- Ruling: Propagate `-Wa,-mbig-obj` only for GNU on Windows through the qualified jsoncons interface. The actual schema translation unit exceeded MinGW's object-section limit; the cost if wrong is an unnecessarily larger object format on that compiler, not a change to schema behavior or other toolchains.
- Ruling: Extend ADR-003's build-tree correction to the three NUL-truncating `allowed_properties.find(prop.key().c_str())` lookups demonstrated by T01. Preserve full-length `prop.key()` instead, retain the failing evidence and add regression coverage for each path. The mandatory NUL-key case cannot pass with the pattern correction alone; the cost if wrong is a bounded, removable dependency patch and qualification rework. No test is skipped and no tracked vendor header is edited.
- Ruling: Include the two equivalent `properties_.find(prop.key().c_str())` defects demonstrated by prefix-collision regressions. Audit the same frozen file once for this class of truncation, preserve full key identity, and test both validation and walk paths. Leaving these defects would misclassify a NUL-containing key as its prefix; the cost if wrong remains a reversible, version-locked patch.
- Ruling: The one-pass audit found two equivalent evaluated-property lookups. Include them in ADR-003 with false/schema-valued unevaluated-property regressions. The final bounded correction is one string-range change plus seven key lookups; diagnostic rendering stays unchanged. Leaving the lookups would corrupt evaluated-key identity; the cost if wrong is removable patch maintenance.
- Ruling: Prepare T02 production CMake integration while T01 finishes its independent correction tests, but defer T02 completion/commit until T01 review passes. Its dependency function/target interface is stable and file ownership is disjoint; the cost if wrong is a small CMake integration rework rather than an unqualified schema claim.
