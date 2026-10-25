# M1 TDD Task Checklist

Status: Ready for execution; all implementation work below is pending. Read [plan.md](plan.md), its interfaces and both implementation ADRs first. Do not mark a task complete from document review alone.

Every task follows RED -> observed failure -> minimal implementation -> GREEN/regression -> review/commit. New Catch2 cases use the `M1.<area>.<behavior>` names below; test discovery prepends `synapse.`. `ctest --test-dir build/m1 --output-on-failure -R '^synapse\.M1\.<area>\.'` is the targeted command after that task creates its cases. Build before each run. Commit only the task's named paths with a short semantic message.

## T01: Qualify the dependency correction

Depends: none. Files (5): `cmake/JsonconsDependency.cmake`, `cmake/ApplyJsonconsPatch.cmake`, `cmake/patches/jsoncons-v1.10.0-pattern-range.patch`, `tests/schema/CMakeLists.txt`, `tests/schema/qualification.cpp`.

- [ ] RED: `pattern_nul` requires schema `{"type":"string","pattern":"z"}` to accept `std::string("a\0z",3)`; NUL-containing property names also work. Require mismatched provenance and missing regex support to fail closed.
- [ ] Run the standalone unpatched qualification build with `cmake -S tests/schema -B build/schema-red -G Ninja -DSYNAPSE_SCHEMA_APPLY_PATCH=OFF`, build it, then `ctest --test-dir build/schema-red --output-on-failure -R pattern_nul`; record the expected failure.
- [ ] Implement the exact pinned fetch, LF/preimage verification and length-aware string/key corrections in ADR-003, including its qualification amendment. The unpatched flag exists only in the standalone qualification project; root SDK builds cannot disable the correction. Make reapplication idempotent only for the exact postimage.
- [ ] GREEN: configure/build `build/schema-green` with correction ON, run its CTest cases and a second configuration; reject a deliberately mismatched copy. No tracked vendor edits.
- [ ] Review and commit `build: qualify schema dependency correction`.

## T02: Verify the downstream build contract

Depends: T01. Files (5): `CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/consumer/CMakeLists.txt`, `tests/consumer/main.cpp`, `cmake/RunConsumer.cmake`.

- [ ] RED: the parent consumer requests C++17, links `synapse::mcp` and compiles `std::expected`; assert `synapse_mcp` is INTERFACE and dependency examples/tests targets are absent.
- [ ] Run the clean consumer commands in plan.md and record the current failure; retain the existing serialization suite.
- [ ] Propagate `cxx_std_23`, use one vendored nlohmann 3.11.3 source, add the qualified jsoncons target, set top-level-only example/test defaults and Catch2 discovery prefix/timeouts. Fail on incompatible/unqualified parent dependency targets.
- [ ] GREEN: clean consumer configure/build plus full `build/m1` CTest; fresh build directory, not a cached success.
- [ ] Review and commit `build: propagate the consumer contract`.

## T03: Define the typed public contracts

Depends: T02. Files (5): `include/synapse/errors.hpp`, `include/synapse/handlers.hpp`, `include/synapse/server_options.hpp`, `tests/contracts.cpp`, `tests/CMakeLists.txt`.

- [ ] RED: `M1.contract.defaults` checks all numeric limits from ADR-002; compile-time assertions pin handler/result/context and `sdk_error` types.
- [ ] Run the contract group and observe missing-interface failures.
- [ ] Add the types and policy hook signatures from plan.md/ADR-002. Keep wire errors distinct; no worker, schema engine or unused executor abstraction in these public contracts.
- [ ] GREEN: contract group and existing protocol tests; standalone include checks for each new public header.
- [ ] Review and commit `feat: define owned server contracts`.

## T04: Decode untrusted wire messages safely

Depends: T03. Files (4): `include/synapse/protocol.hpp`, `include/synapse/detail/message_decoder.hpp`, `tests/message_decoder.cpp`, `tests/CMakeLists.txt`.

- [ ] RED: `M1.protocol.reject_invalid_envelopes` covers non-object/batch input, missing/wrong jsonrpc/method, null/bool/float request IDs, scalar params and ambiguous result/error envelopes. `M1.protocol.roundtrip_ids` preserves string, signed and large unsigned IDs; optional descriptions remain optional.
- [ ] Run the protocol group and retain the observed failures.
- [ ] Implement `decode_message` from plan.md without exceptions escaping; preserve a known valid response ID or use null. Notifications have no response; raw codec compatibility is separate from incoming validation.
- [ ] GREEN: protocol group and the original three serialization cases; no unsigned wrap or stale optional fields on object reuse.
- [ ] Review and commit `fix: validate incoming RPC envelopes`.

## T05: Qualify the offline schema adapter

Depends: T01, T03. Files (4): `include/synapse/detail/schema_validator.hpp`, `tests/schema_adapter.cpp`, `cmake/SchemaTestSuite.cmake`, `tests/CMakeLists.txt`.

- [ ] RED: `M1.schema.registration_and_validation` covers invalid metaschema, absent/unsupported dialect, offline refs/dynamic refs, required unknown vocabularies, unevaluated items/properties, prefix items, branches, NUL cases and unchanged input. Non-finite/binary/discarded SDK JSON never changes silently during conversion.
- [ ] Run schema tests and the pinned official draft2020-12 suite; record failures rather than creating a skip list.
- [ ] Implement ADR-003's exact `compile_schema`/`validate` interface, offline catalog and sanitized failure categories. Format remains annotation by default; no defaults or network/file retrieval.
- [ ] GREEN: every non-optional selected suite case on Linux and Windows, plus adapter regressions. A mandatory failure blocks T11 and requires an explicit recorded correction/decision.
- [ ] Review and commit `feat: add qualified schema validation`.

## T06: Own and cancel application work

Depends: T03. Files (3): `include/synapse/detail/owned_executor.hpp`, `tests/owned_executor.cpp`, `tests/CMakeLists.txt`.

- [ ] RED: `M1.executor.serial_and_bounded` asserts max active callbacks equals 1 and the pending limit is 16. `M1.executor.stop_owns_active_work` gates a callback, requires `wait_for(50ms)==false`, then releases it and checks completion/destruction exactly once.
- [ ] Run executor tests; failures must be assertions or missing symbols, not harness crashes.
- [ ] Implement the queue/cancel/stop/wait contracts from plan.md. Cancel queued calls without invoking them; an active call receives its stop token. Worker-side wait returns `invalid_state`; never self-join or detach.
- [ ] GREEN: executor group with repeated start/stop construction and all earlier tests.
- [ ] Review and commit `fix: own queued application work`.

## T07: Make POSIX byte I/O interruptible

Depends: T03. Files (4): `include/synapse/detail/native_stdio.hpp`, `include/synapse/detail/stdio_posix.hpp`, `tests/posix_io.cpp`, `tests/CMakeLists.txt`.

- [ ] RED: `M1.posix.cancel_idle_read`, `partial_write` and `closed_stdout` exercise private pipe endpoints; the read cancels with the peer still open, all bytes survive partial writes and broken output does not deliver fatal SIGPIPE.
- [ ] Run only the POSIX cases on Linux; mark this path not run on Windows.
- [ ] Implement the common native byte contract and POSIX poll/wake/nonblocking loops. Restore changed descriptor flags and use writer-thread signal masking, not a global SIGPIPE handler.
- [ ] GREEN: POSIX group, repeated endpoint cleanup and earlier tests. Endpoint originals remain valid after owned duplicates close.
- [ ] Review and commit `fix: interrupt POSIX standard streams`.

## T08: Make Windows byte I/O interruptible

Depends: T07's common interface. Files (3): `include/synapse/detail/stdio_windows.hpp`, `tests/windows_io.cpp`, `tests/CMakeLists.txt`.

- [ ] RED: `M1.windows.cancel_before_read`, `cancel_blocked_write` and `handle_cleanup` control the check-to-I/O race and blocked pipes; repeated lifecycles return handle counts to a warmed-up baseline.
- [ ] Run on the declared Windows toolset, not through a POSIX emulation result.
- [ ] Implement owned duplicated pipe endpoints and true Win32 thread handles. Retry cancellation until I/O completion before closing handles; reject unsupported stdio types rather than promising device cancellation.
- [ ] GREEN: Windows group with cancellation before/during I/O and earlier tests; do not cast MinGW native handles or use PeekNamedPipe as a nonblocking guarantee.
- [ ] Review and commit `fix: interrupt Windows standard streams`.

## T09: Bound stdio framing and output

Depends: T07-T08. Files (3): `include/synapse/transport.hpp`, `tests/stdio_transport.cpp`, `tests/CMakeLists.txt`.

- [ ] RED: `M1.stdio.framing` checks fragmented UTF-8 JSON lines, malformed JSON, frame/depth limits, clean stdout and encoded output size. `M1.stdio.backpressure_stop` fills output and requires stop/wait to finish without the peer reading.
- [ ] Run the stdio group using controlled endpoints; retain failure evidence.
- [ ] Implement ADR-002's transport start/send/stop/wait interfaces, reader/writer ownership and close notification. Check limits before unbounded accumulation; flush already-queued output for at most 250ms before aborting SDK I/O.
- [ ] GREEN: stdio group and existing example/client compilation; their compilation does not promote experimental behavior to verified support.
- [ ] Review and commit `fix: bound stdio framing and shutdown`.

## T10: Implement the owned server lifecycle

Depends: T04, T05, T06, T09. Files (5): `include/synapse/server.hpp`, `tests/server_lifecycle.cpp`, `tests/server_ownership.cpp`, `tests/CMakeLists.txt`, `examples/simple_server.cpp`.

- [ ] RED: `M1.lifecycle.negotiate_2025` requires initialize -> `2025-11-25` -> initialized -> ready, counteroffers unsupported versions and answers ping. Pre-ready calls, duplicate in-flight IDs and initialized out of order cannot invoke handlers. Ownership tests assert frozen registration, no post-destruction callbacks and nonblocking callback-side stop. Cancellation tests cover queued/active/late IDs, no notification acknowledgement and no cancellation of initialize.
- [ ] Run lifecycle/ownership groups and observe failures before migration.
- [ ] Use the typed contracts and owned executor; transport/state outlive the owner-thread join. Remove experimental server `send_request`/outgoing promises and detached workers. Handle cancellation as ADR-002 specifies. Update the old server example only as needed to keep compilation consistent.
- [ ] GREEN: lifecycle/ownership groups plus accumulated tests; callback-side wait errors and owner `wait_for` timeouts leave state alive.
- [ ] Review and commit `feat: implement the M1 server lifecycle`.

## T11: Complete the tool slice

Depends: T05, T10. Files (3): `include/synapse/server.hpp`, `tests/tools.cpp`, `tests/CMakeLists.txt`.

- [ ] RED: `M1.tools.validation_and_results` asserts list/call, a text result, empty-object defaults, malformed arguments/unknown tools as RPC errors and schema/domain failures as `isError:true`; callbacks never run on rejected inputs.
- [ ] Run tool tests; the schema qualification gate must already be green.
- [ ] Compile schemas during static registration, enforce MCP root input-schema structure and use immutable validators on the application worker. Map engine failures and exceptions to sanitized internal errors; advertise only the registered scope.
- [ ] GREEN: tools, schema and lifecycle groups; no raw exception/instance value appears in wire diagnostics.
- [ ] Review and commit `feat: validate and dispatch registered tools`.

## T12: Enforce local invocation/output policy

Depends: T11. Files (4): `include/synapse/detail/server_policy.hpp`, `include/synapse/server.hpp`, `tests/server_policy.cpp`, `tests/CMakeLists.txt`.

- [ ] RED: `M1.policy.access_rate_output` requires denied calls not to execute, the 17th same-instant call to hit the 16-token burst, fake-clock refill and control-message responsiveness. Invalid UTF-8/forbidden controls/oversize output are rejected, and a custom filter can redact a known fixture secret.
- [ ] Run policy tests with an injected clock; no sleeps for rate assertions.
- [ ] Apply authorization -> quota -> schema -> callback -> output policy on the single worker. Use ADR-002's `-32050/-32051/-32052` admission errors; preserve `-32002` for resource-not-found. Check final encoded frames and keep exception details private.
- [ ] GREEN: policy/tool tests, bounded queue cases and tests showing custom hooks execute on the application worker.
- [ ] Review and commit `feat: enforce local invocation policies`.

## T13: Complete text resources

Depends: T10, T12. Files (3): `include/synapse/server.hpp`, `tests/resources.cpp`, `tests/CMakeLists.txt`.

- [ ] RED: `M1.resources.text_snapshot` asserts URI/name/MIME list/read, optional metadata, empty template list, missing URI/unknown resource and sanitized callback failure; no subscribe/listChanged claims.
- [ ] Run resource tests and record the current failures.
- [ ] Route registered resource handlers through the same owned worker and output policy; return `contents` text entries and the selected revision's resource-not-found error.
- [ ] GREEN: resources plus lifecycle/policy tests; no unregistered resource callback executes.
- [ ] Review and commit `feat: expose registered text resources`.

## T14: Complete text prompts

Depends: T13. Files (3): `include/synapse/server.hpp`, `tests/prompts.cpp`, `tests/CMakeLists.txt`.

- [ ] RED: `M1.prompts.text_messages` asserts list/get, required/unknown/wrong-type arguments, user/assistant roles and sanitized handler failure; no completion or multimodal claim.
- [ ] Run prompt tests before implementation.
- [ ] Validate argument contracts, dispatch the typed prompt handler on the owned worker and apply output policy to every returned text/description.
- [ ] GREEN: prompts plus resource/policy/lifecycle tests; capability flags match actual registered features.
- [ ] Review and commit `feat: expose registered text prompts`.

## T15: Build the native reference application

Depends: T12-T14. Files (5): `examples/native_status_state.hpp`, `examples/native_status.cpp`, `examples/CMakeLists.txt`, `tests/native_status.cpp`, `tests/CMakeLists.txt`.

- [ ] RED: `M1.reference.real_state` advances a fake monotonic clock and operation count, then asserts matching tool/resource snapshots and an `explain_status` prompt. No fixed machine metrics or dummy hash.
- [ ] Run reference tests and observe missing application behavior.
- [ ] Add the real-state application and executable `native_status`; use explicit app identity, clean stderr diagnostics, inherited stdio and owner-thread lifetime.
- [ ] GREEN: reference tests, executable build and the full unit suite.
- [ ] Review and commit `feat: add the native status integration`.

## T16: Pin the development verifier tools

Depends: T15. Files (4): `tests/interop/package.json`, `tests/interop/package-lock.json`, `tests/interop/.node-version`, `.gitignore`.

- [ ] RED: assert the planned package manifest/lock pins client 2.4.0 and Inspector 2.12.0 and the local runtime is exactly Node 24.22.1; record missing/mismatched tools.
- [ ] Provision a checksum-verified official Node archive under ignored `build/tools/` if needed; use its PATH only for task commands, not a global replacement.
- [ ] Create a dev-only ESM package, exact dependencies and lockfile using `npm --prefix tests/interop install --package-lock-only --ignore-scripts`; ignore only generated node_modules, not the lockfile.
- [ ] GREEN: `npm --prefix tests/interop ci --ignore-scripts`, package/Node version checks and imports from `@modelcontextprotocol/client` and `/stdio`.
- [ ] Review and commit `test: pin independent verifier tools`.

## T17: Verify the official-client flow

Depends: T16. Files (4): `tests/interop/run-client.mjs`, `cmake/InteropTools.cmake`, `CMakeLists.txt`, `tests/CMakeLists.txt`.

- [ ] RED: the harness asserts the actual initialize revision, initialized notification, tool/resource/prompt flow and evidence metadata. A wrong revision/failed operation/missing binary returns nonzero.
- [ ] Run the harness against a controlled wrong-revision fixture before the reference server; record failure without treating transport close as graceful-exit proof.
- [ ] Implement transport recording with the tagged SDK API, 5s request/15s overall limits and scoped cleanup. Add opt-in `SYNAPSE_MCP_BUILD_INTEROP_TESTS`; configure must fail if requested prerequisites are missing. CTest uses absolute generator-resolved server paths and a 30s outer timeout.
- [ ] GREEN: `ctest --test-dir build/m1 --output-on-failure -R '^synapse\.M1\.interop\.'`; evidence identifies versions, OS/compiler/stdlib, source commit and transcript.
- [ ] Review and commit `test: verify exact revision interoperability`.

## T18: Prove shutdown with raw child processes

Depends: T17. Files (3): `tests/interop/process.test.mjs`, `tests/fixtures/process_server.cpp`, `tests/CMakeLists.txt`.

- [ ] RED: subprocess cases cover malformed valid JSON followed by a working ping, idle/cooperative EOF <=2s, stop while stdin remains open, blocked stdout, handle/FD cleanup and a gated non-cooperative callback whose `wait_for` returns false until its test gate is released.
- [ ] Record failures against the old behavior/control fixture. Keep only the test's child handles/PIDs for cleanup.
- [ ] Implement the raw-child harness and fixture. A forced kill is cleanup after a failed graceful test, never a passing shutdown result. Do not use the SDK close escalation path for the EOF proof.
- [ ] GREEN: `ctest --test-dir build/m1 --output-on-failure -R '^synapse\.M1\.process\.'`; every graceful case exits within its budget, and no child is left running.
- [ ] Review and commit `test: prove bounded stdio shutdown`.

## T19: Run the supported CI matrix

Depends: T02, T17-T18. Files (3): `.github/workflows/ci.yml`, `cmake/RecordEnvironment.cmake`, `cmake/Sanitizers.cmake`.

- [ ] RED: verify workflow triggers include `master` push/PR and manual dispatch; demonstrate that a failing unit, schema, consumer or interop case fails its job.
- [ ] Record the baseline missing coverage; resolve action pins from official release SHAs when authoring the workflow rather than inventing them.
- [ ] Implement the Linux container and Windows toolchain pins in plan.md, run build/unit/schema/consumer/process/SDK checks and retain evidence on failures. Use separate ASan+UBSan and supported TSan configurations; never combine TSan with ASan or call a skipped sanitizer successful.
- [ ] GREEN: actual matrix results with compiler/stdlib/runner versions and sanitized logs; no Linux binary is run outside its build container.
- [ ] Review and commit `ci: verify the supported M1 matrix`.

## T20: Verify a named host

Depends: T19. Files (2): `tests/interop/inspector-config.mjs`, `docs/verification/m1.md`.

- [ ] RED: generated read-only config must contain the absolute reference binary path, `protocolEra:'legacy'` and 5s request/connection limits; a record without actual transcript/version evidence is pending, not verified.
- [ ] Run `npm exec --prefix tests/interop -- mcp-inspector --web --config build/evidence/inspector.json` after the generator creates that file. Do not change the user's global MCP catalog.
- [ ] Complete the Inspector Web 2.12.0 tool/resource/prompt walkthrough and export the negotiated `2025-11-25` transcript. Record shared SDK lineage and any environment limitation explicitly.
- [ ] GREEN only with the actual named-host evidence. CLI smoke checks supplement this walkthrough; they do not imply Codex/Claude/VS Code compatibility.
- [ ] Review and commit `docs: record verified host evidence`.

## T21: Publish truthful repository documentation

Depends: T20 and all checkpoint evidence. Files (5): `README.md`, `docs/migration/v0.2-to-m1.md`, `docs/third-party.md`, `docs/verification/m1.md`, `specs/001-native-stdio-foundation/spec.md`.

- [ ] RED: audit README commands against a clean checkout/consumer and every capability claim against AC-01 through AC-08; missing evidence must remain pending.
- [ ] Reproduce installation/build/reference flow with the documented commands, including spaces in the checkout/binary path.
- [ ] Document the text-only API, removed server reverse requests, state/transport lifetime, cooperative shutdown, qualified dependency correction/licenses and exact verified platforms/host. Mark the spec implemented only when all gates pass.
- [ ] GREEN: all acceptance links, commands and local Markdown links verify; full regression and reviewed diff. No performance claim or release publication is implied.
- [ ] Review and commit `docs: document the verified M1 contract`.

## Checkpoint record

- [ ] Dependency/consumer: T01-T03 evidence reviewed.
- [ ] Boundary/schema/executor: T04-T06 evidence reviewed.
- [ ] I/O: T07-T09 evidence reviewed on both supported platforms.
- [ ] Methods: T10-T14 evidence reviewed.
- [ ] Reference/interop: T15-T18 evidence reviewed.
- [ ] Release evidence: T19-T21 meet every acceptance criterion; unavailable checks remain explicit blockers to their support claims.
