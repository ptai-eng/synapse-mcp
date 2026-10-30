# ADR-004: Complete the T05 schema qualification boundary

- Status: Implemented and reviewed; qualified on Windows/Linux locally and in hosted functional, ASan+UBSan and TSan jobs (see ../verification/m1.md)
- Date: 2026-10-26
- Supersedes: ADR-003's NUL-only correction limit and `std::regex` implementation choice; its offline, immutable and sanitized adapter contracts remain binding
- Scope: T01/T05 dependency qualification within the user's T01-T10 completion request

## Evidence and intent

At the initial checkpoint, the adapter ran all 1302 mandatory draft2020-12 cases with zero skips and seven failures on both native MSVC and WSL GCC: one fragment-entry dynamic-scope case and six Unicode property-escape cases. The resumed baseline reproduced those failures (`build/execution/T05-resume-red.log`). The corrected implementation has since passed the unchanged mandatory suite locally on both platforms, including the additional query-bearing dialect regression. Source-quality approval alone is not qualification.

The dynamic-scope diagnosis isolates the defect: a reference directly entering a fragment in another resource bypasses its root object. The current context registers only a visited validator with an explicit `$id`, so a resource's sibling dynamic anchors disappear from that evaluation path. Entering the resource root first changes the result, even though the referenced validation constraints are otherwise equivalent. The regex factories use `std::regex`, which rejects Unicode property escapes. Neither defect can be corrected by filtering errors after validation without changing schema branching semantics.

## Decision

Retain pinned jsoncons v1.10.0 and the existing NUL corrections. Extend the version-locked build-tree correction to these six common backend headers and the seventh factory file identified in the scoped-review amendment below:

1. `keyword_validator.hpp`: preserve full string/key ranges and use the selected Unicode regex engine for pattern storage/search.
2. `keyword_validator_factory.hpp`: construct pattern and pattern-property expressions with that same engine.
3. `format_validators.hpp`: use the same engine for regex syntax checks, retaining format-annotation policy.
4. `schema_validator.hpp`: retain a non-owning association with the enclosing compiled resource root.
5. `schema_validator_factory_base.hpp`: establish resource associations after external resources and reference targets have been compiled/resolved, using canonical resource identity rather than an arbitrary retrieval alias.
6. `eval_context.hpp`: register the associated resource on evaluation transitions, including direct fragment entry, while preserving path-local scope and existing dynamic-target selection.

Resource associations never add validation of a resource root's unrelated keywords. They must not merge anchors across nested `$id` boundaries, invent dynamic anchors, or retain temporary catalog/registry references. Compiled document ownership must keep every associated root alive. Validation and walking use the same resource association.

Qualification amendment (2026-10-27): resource identity removes only the URI fragment; it preserves query and URN r-/q-components. The first complete correction run demonstrated that jsoncons's `uri::base()` removes query information and therefore is unsuitable for this association. Apply the same identity rule to adapter catalog/resource keys where needed. Offline anchor lookup through a retrieval alias must use the loaded document's canonical `$id` and its registered anchors, while preserving nested resource boundaries and rejecting unknown anchors. Add regressions for these distinctions instead of weakening the suite or removing the alias/lifetime fixture.

Scoped-review amendment (2026-10-27): extend the backend correction to a seventh file, `jsoncons_ext/jsonschema/json_schema_factory.hpp`, so custom metaschema resolution also preserves query-bearing dialect identifiers. A confirmed review probe accepts a supported derived dialect without a query but rejects the same preloaded dialect at `?v=1`, because `resolver(uri.base())` discards its identity. Correct the call before information is lost; do not introduce an ambiguous resolver fallback or rewrite schema IDs. Freeze the additional file's pre/postimages and explicitly qualify migration from the exact previous six-header corrected state as well as original/NUL-only states. Partial or unknown states must still fail closed.

Use **SRELL 2026.07**, pinned to Git commit `f57d6162c7c22844727a1fb42c1d15abe0dfb0d7` in the upstream author's linked mirror `upa-url/srell`, as a transitive header-only dependency. Its native UTF-8 ECMAScript Unicode engine replaces regex construction/search inside jsoncons's normal evaluation, including schema combinators. Do not rewrite property escapes into ad hoc character ranges or run a second validation pass. Preserve iterator ranges for embedded NULs. Reject configurations that disable Unicode/property support.

The author documents header-only use, Unicode mode and property escapes in the [SRELL documentation](https://www.akenotsuki.com/misc/srell/en/). The pinned [license](https://github.com/upa-url/srell/blob/f57d6162c7c22844727a1fb42c1d15abe0dfb0d7/srell-src/license.txt) is BSD-2-Clause. Retain that license, header copyright notices and generated Unicode-data notices. Do not run the mirror's downloader, Unicode generators or upstream test project; its CMake project unconditionally creates tests and changes the C++ standard. Expose only an owned INTERFACE header target.

SkillSpector scanned the mirror's current HEAD, which resolves to the exact pinned commit, and reported SAFE / LOW (11/100). Its heuristic findings concern license wording, generated Unicode names and generator state; the selected regex headers perform in-memory matching. The downloader/generators are excluded from the build. The scan is an audit aid, not proof of dependency correctness.

## Provenance and verification

Verify the exact commit and reviewed preimage/postimage hashes for every corrected file, not only `keyword_validator.hpp`. Accept only a complete original state, the exact previously qualified NUL-only state for forward migration, or the complete new postimage. Reject unknown edits and partially applied corrections. Also reject modified/untracked dependency content outside the allowlisted correction and unqualified parent/source overrides. Reconfiguration must recheck provenance, and reapplication is a no-op only on the complete exact postimage. Freeze the SRELL header identities and fail on source drift.

The following completion requirements were met; results are recorded in the [verification record](../verification/m1.md). Require these checks again after relevant implementation changes: targeted RED/GREEN regressions for fragment/pointer/anchor entry, nested resource boundaries and compiled-state lifetime; Unicode property/complement patterns, full UTF-8 code points, invalid expressions, embedded NULs and schema combinators; correction idempotence, tampering and consumer propagation. Run all 46 mandatory suite files / 384 groups / 1302 cases with zero skips and zero failures on native Windows and Linux. Run accumulated T01-T10 checks and independent review. Keep the gate failing until the results establish success.

This decision adds dependency/compile-time cost and a maintained six-file backend patch. If the resource association or regex choice is wrong, qualification and downstream consumer tests must fail before tool migration. Remove the corrections when a pinned upstream implementation provides equivalent behavior and passes the same gates. It does not authorize T11+, a protocol revision change, publication or README work.
