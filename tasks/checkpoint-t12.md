# T11-T12 tool and policy checkpoint

Date: 2026-10-28. Branch: `codex/product-protocol-foundation`.
Implementation: `fce5e1b` and `8a39fc1`; commits remain local.

T11 and T12 are implemented, tested locally and independently reviewed. T13-T21 and README work remain pending; this checkpoint does not establish full M1, official-client or named-host compatibility.

## Behavior

- Register tools before startup with an object-root JSON Schema; compile and own validators during registration. Duplicate, invalid and late registrations fail locally without publishing a partial tool.
- Advertise tools when registered, list their descriptors and call handlers on the single owned application worker. Missing arguments default to an empty object. Unknown tools and malformed arguments use JSON-RPC invalid-params errors; schema mismatch and public domain failure use `isError: true`. Unexpected exceptions and engine failures return sanitized internal errors.
- For registered calls, apply authorization, then the per-server token bucket, schema validation, handler and output policy. Defaults are a 16-token burst and 16 tokens per second. Denied calls do not consume quota; authorized calls with schema mismatches do. Control messages remain responsive while a policy hook is blocked.
- Authorization and output hooks execute on the same owned worker as handlers. Output must be valid UTF-8, reject forbidden controls and fit the final encoded frame after escaping; filtering may redact application text. Rejected tool output becomes a generic `isError` result, while resource output rejection becomes a generic internal error.
- Access denial and rate limiting use `-32050`/`access_denied` and `-32051`/`rate_limited`. The existing queue-full `-32052`/`queue_full` and resource-not-found `-32002` remain separate.
- `stdio_transport::max_encoded_frame_bytes()` exposes its configured single-frame limit, excluding LF. For custom transports without this optional accessor, the server uses the default 1 MiB frame cap. If even a sanitized fallback cannot fit because of a long request ID, the server stops instead of sending an oversized response.

## Evidence

| Check | Result |
|---|---|
| Windows 11 / native MSVC 19.44.35228.0 | 48/48 selected CTest cases pass |
| Ubuntu WSL / GCC 15.2.0 | 46/46 selected CTest cases pass |
| Selection on both platforms | Tool, policy, lifecycle, ownership, schema including the official mandatory gate, and stdio |
| Policy executable on MSVC | 5 cases / 68 assertions pass |
| Long-ID fallback regression | RED reproduced the missing stop; GREEN passes without emitting an oversized frame |
| Raw stdio child process, Windows GNU | Actual revision negotiation, tool listing/call, schema rejection, authorization denial, output redaction, ping and cooperative EOF pass |

The raw child-process check is a local integration smoke test, not official-client or host interoperability evidence. Full M1 verification, the supported CI matrix and named-host walkthrough remain later tasks.

T11's scoped review approved spec and quality without actionable findings. T12's independent review identified the long-ID fallback issue; the targeted fix and rereview passed. Reports and logs are retained locally under ignored `build/execution/` (`T11-review.md`, `T12-review.md`, `T12-fix-review.md`, `T11-T12-{windows,linux}-{build,test}.log`, and `T11-tool-wire-windows.json`). These local evidence files are not committed.

## Re-run the selected checks

Windows:

```powershell
cmake --build build/t10-msvc --config Debug --target synapse_policy_tests synapse_tool_tests synapse_server_tests synapse_schema_adapter_tests synapse_stdio_tests --parallel 2
ctest --test-dir build/t10-msvc -C Debug --output-on-failure -R '^synapse\.M1\.(policy|tools|lifecycle|ownership|schema|stdio)\.'
```

Linux (inside the configured Ubuntu WSL tree):

```sh
cmake --build build/t07-linux --target synapse_policy_tests synapse_tool_tests synapse_server_tests synapse_schema_adapter_tests synapse_stdio_tests --parallel 2
ctest --test-dir build/t07-linux --output-on-failure -R '^synapse\.M1\.(policy|tools|lifecycle|ownership|schema|stdio)\.'
```

These commands reuse configured local build trees. No push, PR, release or global MCP configuration was performed.
