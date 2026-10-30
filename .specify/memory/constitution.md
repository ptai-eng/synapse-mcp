# Synapse MCP Constitution

- Version: 1.1.0
- Ratified: 2026-10-25
- Last amended: 2026-10-25

## 1. Serve native application developers

Synapse MCP helps C++ developers expose existing application operations and data to MCP hosts. The first supported integration is a host-launched local stdio server. Prioritize the server integration path before expanding the SDK client or network adapters.

The accepted product and protocol decision is [ADR-001](../../docs/decisions/0001-product-and-protocol.md); the bounded first milestone is [specification 001](../../specs/001-native-stdio-foundation/spec.md).

## 2. Keep the embedding contract small

Require C++23 and a standard library providing the facilities used by the public API, including `std::expected`. Preserve a header-only core and the `synapse::mcp` CMake target. Propagate language and dependency requirements to downstream consumers.

Name dependencies and their build requirements. Keep future HTTP stacks, model providers and application frameworks outside the core. Do not edit vendored implementations to solve Synapse defects.

A demonstrated dependency defect may use a narrowly scoped, version-locked correction applied only to downloaded build-tree sources. Record the upstream provenance, exact correction, regression gate, license obligations and removal condition in an ADR. Verify the preimage/postimage and fail on a mismatch. This exception does not allow editing tracked vendor copies, unrelated dependency changes or unqualified compatibility claims. [ADR-003](../../docs/decisions/0003-schema-validation.md) records the first such decision.

## 3. Specify protocol behavior before claiming compatibility

Use an explicitly dated MCP revision and a documented feature profile. A revision target is a design decision; verified support requires tests and versioned interoperability evidence. Never infer support from a version string, class name or capability flag alone.

Advertise only capabilities whose declared behavior works. Distinguish protocol errors, tool execution errors and transport failures. Validate untrusted messages at the boundary; an exception from decoding or an application callback must not escape an SDK-owned thread.

## 4. Own work and resources through shutdown

Every worker, pending request and transport resource must have an owner and a cleanup path. Work must not outlive the state it accesses. Document callback execution context, application-state lifetime and cooperative cancellation before promising thread safety or bounded shutdown.

The initial server profile freezes registration before serving. Dynamic registration and concurrent registry mutation require a separate design and tests. Do not claim that arbitrary non-cooperative application callbacks can be forcibly stopped safely.

## 5. Let evidence determine public claims

Changes to behavior need tests at the public boundary. Include malformed messages, callback failures, EOF, shutdown and consumer integration where relevant. Existing serialization tests do not establish server or transport correctness.

Publish the compiler, standard library, OS, client/host version and negotiated protocol revision for an interoperability result. Make performance or stability claims only when reproducible measurements support them. Mark experimental surfaces and API breaks explicitly.

## Governance

Use this constitution for implementation plans and reviews. Record product-scope or protocol changes in a new ADR that supersedes the old decision; do not silently reinterpret accepted decisions. Update the milestone specification when its scope changes.

Routine implementation and verification within delegated scope can proceed autonomously. Follow the user's authorization for publication and other external actions. Do not remove user data, discard unrelated work or include local analysis archives in product commits.
