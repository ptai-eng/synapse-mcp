# T20 named-host checkpoint

Date: 2026-10-29. Status: complete. Named-host evidence and generator passed independent specification and quality review with no actionable findings.

## Executed host

The user operated MCP Inspector Web 2.12.0 in Microsoft Edge on Windows, using a generated read-only session config and the fresh T21 reference binary. Browser Use's loopback origin policy continued to deny automated access; no alternate surface or policy workaround was used. The user performed the walkthrough and exported the Protocol Messages history.

The original export is `inspector-protocol-native_status-2026-10-29T12-02-28.613Z.json` in the workspace root; it remains unchanged. It records 25 entries: 24 requests with matching response IDs and one `notifications/initialized` notification. Initialize offers and receives `2025-11-25`, identifying Inspector 2.12.0 and server `native-status` 0.2.0. No request contains an RPC error.

Discovery lists the tool `app.status`, resource `app://status` and prompt `explain_status`. The tool call returns `handled_operations: 1`; both subsequent resource reads and the prompt retain that count while uptime advances. These are actual application-state results. The trace is an Inspector export with paired responses, not an independent byte-level capture or shutdown test.

## Identity and limits

The generated config references `build/T21 reference build/examples/Debug/native_status.exe`. Its SHA-256 is `ecaa47bbe381793477133b802a080e27a3aedbf3e26cd2f5919a817079f20c24`, matching the [T21 clean-checkout reproduction](checkpoint-t21.md) at source `1fa654be73d582e9ae4778da0d757e9112a6bac1`. That retained checkout/configure/build chain supplies attribution; the standalone binary does not embed source attestation.

Node is 24.22.1; the recorded build uses MSVC 19.44.35228.0 and STL 143/update 202503 on Windows x64 10.0.26200. Inspector's internal official client 2.3.1 shares SDK lineage with the separate official-client 2.4.0 harness. This is verification of this Inspector host/version, not proof for Codex, Claude Desktop, VS Code or every MCP feature.

The Inspector pagination switch attempted to save a setting in a read-only session and produced a read-only error. It does not invalidate the successful list/call/read/get trace, and no server pagination behavior is claimed from that switch.

## Scope decisions

Ruling: retain a credential-checked exported transcript alongside the verification page in addition to T20's original two paths. This makes actual named-host evidence reviewable from the repository instead of depending on ignored local logs. Cost if wrong: one small evidence fixture to maintain; no runtime or API expansion.

README remains unchanged. Full T21 and AC-08 remain pending until the user authorizes the README pass. Do not infer full M1 completion from this host checkpoint.

## Final checks and review

The [retained export](../docs/verification/evidence/inspector-20261029.json) is byte-identical to the original (SHA-256 56e69abf8c90691454499d206fa9840cf7ffdb74b6c50acac83b047318b45480), with no credentials or local paths found in the inspected payloads. The focused checker passed and rejected eight deliberate evidence mutations. Generator contract checks passed; 47 local links/anchors and diff checks passed. Independent review approved specification compliance and quality, checking transcript/config/binary hashes and unchanged README. Detailed records are ignored build/execution/T20-final-report.md and T20-final-review.md. No full runtime regression was repeated for these documentation/verifier changes.
