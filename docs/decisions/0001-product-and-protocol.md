# ADR-001: Native stdio servers and MCP 2025-11-25

- Status: Accepted design; implementation pending
- Date: 2026-10-25
- Applies to: the first verified native stdio milestone, M1

## Decision

Synapse MCP is a **server-first, header-only C++23 SDK for exposing native application operations and data to MCP hosts**. The first supported integration is a local executable launched by the host and communicating over stdio.

Pin **MCP `2025-11-25` as M1's single supported revision target**. Track `2026-07-28` as a separate migration milestone. Do not implement dual-era negotiation or promise fallback across revisions in M1.

This decision does not change the running code. The implementation assessed at `22c2126` still hard-codes `2024-11-05` and has known lifecycle defects. Neither the selected target nor current protocol conformance is established by this documentation commit. [Specification 001](../../specs/001-native-stdio-foundation/spec.md) defines the evidence required to claim support.

## Context and primary user

The primary user owns a native C++ application, developer tool or domain library and wants an MCP host to inspect its state or invoke explicitly registered operations. They need to reuse domain code with a small integration surface, predictable object lifetimes and a reproducible build.

A host launches an executable that links the native domain code and dedicates stdin/stdout to MCP. An application may offer an explicit `--mcp` mode if it can honor that stream contract. Integration with arbitrary GUI event loops is outside M1.

The existing server registry, transport separation and CMake interface target support this direction. Public `std::expected` use already requires C++23. The SDK client, subprocess transport and SSE adapter exist, but their presence is not a verified product-support promise.

## Why this revision

The latest published revision checked on this date is `2026-07-28`. It changes lifecycle and wire contracts, rather than merely adding optional methods. We are deliberately selecting an earlier published revision for a bounded release. [Current specification](https://modelcontextprotocol.io/specification/latest).

Current TypeScript SDK documentation keeps initialization-based behavior as the default and makes modern negotiation opt-in. Inspector also defaults to legacy, with explicit modern and automatic modes. These facts establish available interoperability paths; they do not prove adoption by every installed MCP host. [TypeScript migration guide](https://github.com/modelcontextprotocol/typescript-sdk/blob/main/docs/migration/support-2026-07-28.md), [Inspector protocol eras](https://modelcontextprotocol.io/docs/2026-07-28/tools/inspector/protocol-eras).

`2025-11-25` retains the lifecycle model already present in Synapse, allowing the next work to address ownership, validation and shutdown with one wire profile. The reasoning is a scope and integration trade-off, not a claim that newer protocols are unstable.

## Alternatives considered

| Option | Benefit | Cost | Outcome |
|---|---|---|---|
| Repair only `2024-11-05` | Smallest immediate protocol change | Keeps the first verified milestone on an older contract without addressing the intended next baseline | Retain as historical behavior, not a supported M1 profile |
| Target only `2025-11-25` | One lifecycle and wire profile; explicit legacy client path | A later migration is required for modern-only clients | Accepted for M1 |
| Target `2026-07-28` immediately | Aligns with the current specification | Replaces lifecycle and result semantics while basic reliability remains unresolved; modern mode is not universal | Deferred |
| Support both 2025 and 2026 now | Broader potential client coverage | Two distinct protocol paths and a larger verification matrix | Deferred until there is verified demand and capacity |

For product scope, a symmetric client/server SDK would spread verification across both connection roles. A general agent framework would introduce model and application concerns beyond the existing library. M1 instead verifies the native server use case; other surfaces remain experimental.

## Compatibility and version policy

| Revision or surface | Current implementation | M1 commitment |
|---|---|---|
| `2024-11-05` | Partial, with known defects; not certified | No compatibility guarantee or fallback |
| `2025-11-25` | Migration not implemented | Single target; support claim gated by M1 acceptance |
| `2026-07-28` | Not implemented | Deferred migration; no modern-mode claim |
| SDK client/process/SSE APIs | Experimental | Existing surfaces are not promoted to supported status by M1 |

During initialization, return `2025-11-25` when requested. If an otherwise valid `initialize` proposes any unsupported revision, including `2026-07-28`, counteroffer the only supported target and let the client decide whether it can continue. Do not echo an unsupported revision or enable modern semantics. A counteroffer does not implement the revision originally proposed. A modern-only client opening with `server/discover` will not work until the later migration. [Selected lifecycle](https://modelcontextprotocol.io/specification/2025-11-25/basic/lifecycle).

API changes needed for M1 may break the experimental v0.2 API. Document those changes and provide a migration note before release. Keep tagged historical releases available; do not relabel old binaries as supporting the new target.

## Consequences and migration trigger

M1 prioritizes the stdio server path, truthful capability declarations and a versioned test transcript. It does not add HTTP, OAuth, a production SDK client, sampling, roots or protocol-level logging. Application diagnostics use stderr.

The implementation plan must preserve a clear boundary between message codecs, connection lifecycle and registered application handlers. This is a design constraint, not a request to build unused abstractions now.

Open the 2026 migration after M1 has a passing release evidence record and a concrete modern-client use case. Its new ADR must assess discovery, per-request metadata, result/cache fields, message direction and subscriptions. It must name the independent client/host used for verification. [2026 changes](https://modelcontextprotocol.io/specification/2026-07-28/changelog), [Versioning and compatibility](https://modelcontextprotocol.io/specification/2026-07-28/basic/versioning).

## Decision completion

This decision step is complete when the primary user, integration path, C++ baseline, exact revision, exclusions, compatibility policy and release evidence gates are recorded consistently in the constitution, milestone specification and repository status. Runtime implementation follows as a separate step.
