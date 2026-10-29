# Foundation handoff: T01-T21 authorized scope

Date: 2026-10-29. T01-T20 and the user-authorized non-README portion of T21 are complete and reviewed. The user reaffirmed that README should wait and will create any release later. README/AC-08 therefore remains a separate deferred acceptance gate; this handoff does not declare the original full M1 acceptance complete.

| Scope | Reviewed evidence |
|---|---|
| T01-T10: dependency/build contract, schema, executor, stdio and protocol foundation | [T10 checkpoint](checkpoint-t10.md) and [execution ledger](execution.md) |
| T11-T15: policy, tools, resources, prompts and native reference | [T12](checkpoint-t12.md), [T15](checkpoint-t15.md), [thorough audit and repairs](audit-t11-t15.md) |
| T16-T17: pinned verifier and exact-revision official client | [T17 checkpoint](checkpoint-t17.md) |
| T18: raw-process recovery, shutdown and lifecycle/resource checks | [T18 checkpoint](checkpoint-t18.md) |
| T19: actual hosted functional and sanitizer matrix, failure controls | [T19 checkpoint](checkpoint-t19.md) |
| T20: user-operated Inspector Web host and exported transcript | [T20 checkpoint](checkpoint-t20.md) and [retained transcript](../docs/verification/evidence/inspector-20261029.json) |
| T21, README excluded: migration, dependency/licenses, verification and spec | [T21 checkpoint](checkpoint-t21.md) and [verification record](../docs/verification/m1.md) |

The unchanged runtime at `1fa654be73d582e9ae4778da0d757e9112a6bac1` passed hosted Windows 118/118 and Linux functional, ASan+UBSan and TSan 117/117 per job. Four deliberate failing controls verified CI failure propagation. Inspector Web 2.12.0 negotiated `2025-11-25` and completed the native tool/resource/prompt flow. Its shared SDK lineage and binary/source attribution limits remain explicit in the verification record.

The latest consistency pass corrected stale Inspector and ADR status labels. A separately prepared README draft passed fresh Windows reference/consumer builds, compiled C++/CMake sample and official-client reproduction, but the user's latest instruction defers it. The draft and logs are preserved under ignored `build/execution`; committed README content remains unchanged. No additional runtime edits or duplicate full-suite run were needed for these documentation changes.

Inspector cleanup stopped only the Node processes carrying this task's exact manual-session config path. A subsequent check found zero matching processes and no listeners on ports 6274/6275. The user's PowerShell window was left open.

No release, tag, version bump, merge or push was performed for this handoff. The user's `.understand-anything/` directory and original Inspector export remain untouched.
