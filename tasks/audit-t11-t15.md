# T11-T15 thorough audit

Date: 2026-10-28. Branch: `codex/product-protocol-foundation`.

The original T11-T15 implementation checkpoints passed. This follow-up audit found three integration defects that were outside the earlier cases; prior green results did not waive them. Scoped rereview approves the corrected source and tests. Final platform verification completed on 2026-10-29.

## Findings and corrections

| Finding | Correction | Regression |
|---|---|---|
| A1: Reader/control/admission responses bypassed the custom transport frame limit | Centralize final encoded-frame admission for every response | Pre-ready, queue-full, initialization and decoder responses with small frame capacities |
| A2: Oversized output from a cancelled operation could close unrelated work | Decide cancellation and response admission under the same server mutex | Gate an active operation, cancel it, release oversized output and require unrelated work and ping to succeed |
| A3: Resource registration/read accepted malformed URI syntax | Validate absolute RFC 3986 syntax without resolving, decoding or normalizing resource identity | Invalid syntax rejected locally or with -32602; valid unknown URI remains -32002; custom schemes, escaped octets and IP literals remain supported |

The three new cases reproduced the defects before production edits: 22 failing assertions. The expanded standalone policy/resource suite then passed 9 cases with 288 assertions. Scoped rereview additionally found unsafe assertion-unwind ordering in two new fixtures; their captured gate state now outlives the server join. A second regression also proves that an oversized initialize response cannot leave a usable session after its error fallback. The final standalone selection passed 16 cases with 418 assertions.

## Verification

- Baseline full suites: Windows/MSVC 104/104 and Ubuntu WSL/GCC 103/103, with downstream consumer and actual stdio probes passing.
- Final Windows/MSVC full unfiltered CTest: 107/107. Downstream consumer and real stdio/native-state probes passed. The 30 focused lifecycle/ownership/tool/policy/resource/prompt/reference cases each passed 10 consecutive executions (300 successful executions).
- Final Ubuntu WSL/GCC full unfiltered CTest: 106/106. Downstream consumer and real stdio/native-state probes passed. The same 30 focused cases each passed 10 consecutive executions (300 successful executions).
- Scoped rereview: spec and quality APPROVE; all A1-A3 and the two follow-up defects addressed, no open findings. Source commit: `682e4a9`.

Detailed local evidence is under ignored `build/execution/`: `T11-T15-audit-review.md`, `T11-T15-audit-fix-report.md`, `T11-T15-audit-fix-review.md`, and `T11-T15-audit-fix2-final-*`.

## Scope

The review covered tool registration/schema/result errors, authorization/quota/output policy, resource and prompt contracts, combined output frame handling, cancellation and callback lifetime, and native reference state. This does not establish named-host, CI or full M1 qualification. The separate [T17 checkpoint](checkpoint-t17.md) records the subsequently completed official-client verification. README remains T21.
