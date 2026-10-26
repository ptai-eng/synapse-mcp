# ADR-003: A pinned JSON Schema backend with an explicit dependency correction

- Status: Adapter implemented and reviewed; qualification BLOCKED (7/1302 mandatory cases fail)
- Date: 2026-10-25
- Qualification checkpoint: [T10 review record](../../tasks/checkpoint-t10.md), 2026-10-26
- Applies to: M1's MCP `2025-11-25` tool-input validation
- Related: [ADR-001](0001-product-and-protocol.md), [M1 specification](../../specs/001-native-stdio-foundation/spec.md), [constitution](../../.specify/memory/constitution.md)

## Decision

Use **jsoncons v1.10.0**, pinned to commit `9e57e421285422d7aeeb5aa7dc839f6a19121ee6`, through an internal validation adapter. Keep `nlohmann::json` as the protocol and application-facing JSON type, including the current vendored 3.11.3 header. Do not replace or edit `include/nlohmann/`.

Apply narrowly scoped, tracked corrections to jsoncons's downloaded build-tree source: make the `pattern` validator's regular-expression search use the full string range, including embedded NUL characters, and preserve the full property-key length in `additionalProperties`, `properties` and `unevaluatedProperties` lookups. Preserve the upstream license and notices. Do not apply a broad regex rewrite, modify tracked vendor headers, or silently patch system-installed dependencies.

The pinned source and correction are **candidates until the qualification gate passes**. Neither upstream's supported-draft documentation nor this ADR establishes that Synapse currently provides JSON Schema 2020-12 validation. Tool migration must wait for the gate.

## Context and alternatives

The selected MCP revision requires JSON Schema 2020-12, including the default dialect when `$schema` is absent. A draft-07 validator cannot satisfy that contract by changing its advertised version. [MCP schema requirements](https://modelcontextprotocol.io/specification/2025-11-25/basic/index).

jsoncons supplies a header-only 2020-12 engine and keeps the core free of a new compiled runtime. Its separate JSON representation adds conversion, allocation and compile-time cost. Accept that cost explicitly; keep the adapter replaceable and make no performance claim. [jsoncons schema documentation](https://github.com/danielaparker/jsoncons/blob/v1.10.0/doc/ref/jsonschema/jsonschema.md).

| Alternative | Decision |
|---|---|
| Unmodified jsoncons v1.10.0 | Not qualified: source inspection found a NUL-truncating pattern search |
| jsoncons with a version-locked build-tree correction | Selected for qualification; preserves the header-only integration model |
| pboettch/json-schema-validator | Not selected: its documented schema baseline is draft-07 and it adds a compiled library |
| Sourcemeta Blaze | Not selected as the default dependency: its current distribution is AGPL/commercial rather than a permissive dependency for this SDK |
| A new validator or a second pattern-validation pass | Not selected: unnecessary engine scope; a post-pass can change `anyOf`, `oneOf`, conditional and unevaluated semantics |

Sources: [pboettch validator](https://github.com/pboettch/json-schema-validator), [Blaze license](https://github.com/sourcemeta/blaze/blob/main/LICENSE).

## Exact dependency and correction boundary

The stable v1.10.0 release was published on 2026-10-05. Its license is **Boost Software License 1.0**. The Git blob identity for `include/jsoncons_ext/jsonschema/common/keyword_validator.hpp` is `45e9040c2b65e44bb102c712ea91f49fe2b0a2b0`. An archive SHA-256 has not been verified; do not invent one or label the Git blob identity as an archive checksum. Fetch the exact commit, not a moving branch. [Release](https://github.com/danielaparker/jsoncons/releases/tag/v1.10.0), [tag identity](https://api.github.com/repos/danielaparker/jsoncons/git/ref/tags/v1.10.0), [license](https://github.com/danielaparker/jsoncons/blob/v1.10.0/LICENSE).

In that source file, line 643 searches `instance.as_cstring()`. It can stop at an embedded NUL: schema `{"type":"string","pattern":"z"}` must accept the JSON string `"a\u0000z"`. The correction obtains `instance.as_string_view()` and passes its begin/end range to `std::regex_search`. It changes no schema branching or regex syntax. The adapter emits normalized errors instead of exposing upstream messages containing raw instance values. [Pinned validator source](https://github.com/danielaparker/jsoncons/blob/v1.10.0/include/jsoncons_ext/jsonschema/common/keyword_validator.hpp).

The `patternProperties` searches at lines 2757 and 2805 already receive `std::basic_string` keys, as does their regex factory. Do not patch them as C-string searches; cover embedded-NUL keys with regression tests. [Key representation](https://github.com/danielaparker/jsoncons/blob/v1.10.0/include/jsoncons/key_value.hpp), [key policies](https://github.com/danielaparker/jsoncons/blob/v1.10.0/include/jsoncons/basic_json.hpp).

### Qualification amendment: preserve property-key length

The first actual qualification run exposed a second defect in the same frozen file. `additional_properties_validator` stores full-length property names but looks them up with `allowed_properties.find(prop.key().c_str())` at original lines 2909, 2941 and 3025. A key `std::string("a\0z",3)` matching `patternProperties: {"z": {"type": "integer"}}` is consequently rejected by `additionalProperties: false`. This is distinct from the regex searches above.

Replace only those three lookups with `allowed_properties.find(prop.key())`. Extend the version-locked correction and its exact whole-file postimage guard together. Qualify the affected additional-property paths with embedded-NUL matching and nonmatching keys; retain the observed failing first-correction result. This amendment adds no dialect or schema feature and authorizes no other dependency edits. Remove both corrections when an upstream release contains equivalent fixes and passes the same qualification gate. Evidence: `build/execution/T01-report.md` records the local failing run and source diagnosis; it is an execution artifact, not a public compatibility claim.

The prefix-collision regression also demonstrates the same truncation in the two `properties_.find(prop.key().c_str())` paths used by `properties_validator` validation and walking. Include the corresponding length-preserving `prop.key()` replacements and regressions distinguishing a NUL-containing property from its prefix. These corrections remain restricted to string/key length; no schema branching is changed.

A one-pass audit of this file found two equivalent `results.evaluated_properties.find(prop.key().c_str())` lookups at original lines 3930 and 3966. Preserve the full key there too, and test both false-valued and schema-valued `unevaluatedProperties`. The complete correction boundary is one string-range change plus seven key lookups. Remaining C-string uses in diagnostic rendering are left unchanged because the adapter never returns those raw messages.

The implementation task must make correction application reproducible and fail closed:

1. Fetch the pinned commit into a dependency build tree with LF checkout (`core.autocrlf=false`). Verify the original file's Git blob identity with `git hash-object --no-filters` before its first modification.
2. Check that the expected preimage occurs exactly once. Apply only the reviewed correction. Verify the exact expected postimage and record its digest in the correction's regression fixture.
3. A second application is a no-op only when that exact postimage is verified. A different source version, mixed state or unmatched preimage/postimage is a configuration error.
4. Keep the correction and tests in Synapse's own build-support files, with upstream provenance and the reason for removal when a qualified upstream release fixes the defect. Leave copyright/license text intact.
5. Require `JSONCONS_HAS_STD_REGEX` in the adapter. The backend has a regex-disabled path that cannot establish `pattern` validation; reject that configuration at compile time.

This explicit dependency-bug correction follows the constitution's recorded dependency-correction policy. It is not permission for unrelated changes to third-party implementations.

## Build integration

Upstream's CMake floor is 3.15; Synapse retains its 3.20 floor. The fetched project defines the **`jsoncons` INTERFACE target**. Installed exports use the `jsoncons::` namespace, while the fetched build does not create that alias automatically. Normalize the owned target for Synapse's interface dependency; do not assume the alias exists. Set the upstream `JSONCONS_BUILD_TESTS` option off before making the dependency available, because it defaults on. [Pinned CMake configuration](https://github.com/danielaparker/jsoncons/blob/v1.10.0/CMakeLists.txt).

Reject a pre-existing jsoncons target whose provenance and correction have not been qualified. Do not silently reuse an unpatched installation with the same version number, or modify another project's dependency copy. Downstream consumer tests must verify propagated include/dependency requirements. Preserve BSL-1.0 notices when headers are distributed.

## Internal adapter contract

These are implementation-plan interfaces in `synapse::detail`, not a new public compatibility promise. `sdk_error` is the SDK's planned configuration/runtime error type, separate from JSON-RPC wire errors.

```cpp
using schema_catalog = std::map<std::string, nlohmann::json>;

enum class validation_failure_kind {
    instance_mismatch,
    invalid_instance,
    engine_failure
};

struct validation_failure {
    validation_failure_kind kind;
    std::string instance_pointer;
    std::string schema_pointer;
};

class compiled_schema {
public:
    std::expected<void, validation_failure>
    validate(const nlohmann::json& instance) const;
};

std::expected<compiled_schema, sdk_error>
compile_schema(const nlohmann::json& schema,
               const schema_catalog& catalog);
```

Compile schemas during registration and keep compiled state immutable once serving begins. The compiled object owns its backend state. Convert nlohmann JSON to jsoncons only inside the adapter; do not expose backend types through server callbacks. Reject binary/discarded/non-finite values before serialization can silently change them. Catch normal backend exceptions at this boundary and return the typed failure category.

Use the categories distinctly: `instance_mismatch` means a valid JSON instance failed its declared tool contract; `invalid_instance` means the supplied SDK value cannot represent valid wire JSON; `engine_failure` means validation did not produce a validity answer. Never turn an engine failure into a successful tool invocation. Error-to-wire policy belongs to the server task and must preserve the spec's protocol-error versus tool-execution-error distinction. No returned diagnostic contains raw instance values or unsanitized upstream exception text.

## Dialect, metaschema and reference policy

- Default explicitly to `jsoncons::jsonschema::schema_version::draft202012()`. M1 targets 2020-12; reject unsupported declared dialects with a clear registration error, including declarations in referenced schema resources. A preloaded metaschema deriving from 2020-12 may select supported vocabularies or optional unknown vocabularies; resolve it offline and inspect its required vocabularies rather than rejecting its URI alone. This preserves the mandatory vocabulary-suite cases without allowing network retrieval or unsupported required vocabularies.
- Validate a registration schema against the bundled 2020-12 metaschema before compiling it for instance validation. `make_json_schema` compiles an evaluator; that operation alone is not proof that the registration schema is valid.
- Keep access to the pinned backend's bundled metaschema inside the adapter: `jsoncons::jsonschema::meta_resolver<jsoncons::json>(uri)` supplies it without network access. Regression-test this pinned helper rather than exposing it as a public SDK API.
- Use `make_json_schema(root, resolver, options)` with an offline resolver of signature `jsoncons::json(const jsoncons::uri&)`. It serves only built-in metaschemas and preloaded `schema_catalog` documents; unknown references return no resource and fail registration. HTTP-looking URI identifiers do not authorize HTTP requests. Never open a URI as a local filename.
- Preserve `$defs`, local/external registered `$ref` and `$dynamicRef` semantics. Unknown required vocabularies must fail registration; optional unknown vocabularies follow the selected dialect's rules. Do not silently claim an unsupported required vocabulary.
- Keep `require_format_validation(false)` and compatibility mode off for the default 2020-12 profile. `format` is an annotation by default. A schema requiring an unsupported assertion vocabulary must fail clearly. Do not apply backend-generated patches or populate `default` values during validation.

Sources: [evaluation options](https://github.com/danielaparker/jsoncons/blob/v1.10.0/include/jsoncons_ext/jsonschema/evaluation_options.hpp), [factory and bundled resolver](https://github.com/danielaparker/jsoncons/blob/v1.10.0/include/jsoncons_ext/jsonschema/json_schema_factory.hpp), [bundled metaschema](https://github.com/danielaparker/jsoncons/blob/v1.10.0/include/jsoncons_ext/jsonschema/draft202012/schema_draft202012.hpp).

## Qualification and release gate

Qualification has **not been executed** in this planning step. The implementation must first reproduce the uncorrected NUL-string failure, then demonstrate the corrected result and run the relevant official 2020-12 test-suite cases on the declared Linux and Windows toolchains.

The gate includes malformed registration schemas, absent/explicit/unsupported dialects, unresolved offline references, required unsupported vocabularies, `$defs`/`$ref`/`$dynamicRef`, `prefixItems`, `unevaluatedProperties` and `unevaluatedItems`, branch combinations, numeric boundaries, regex compilation errors, NUL-containing strings and keys, and the regex-disabled build rejection. Check that validation leaves input JSON unchanged and that diagnostic values remain sanitized.

A failed mandatory case blocks tool migration and any 2020-12 support claim. Do not remove the case, reinterpret invalid engine output as valid, or substitute an undocumented validator. Resolve the dependency decision explicitly before proceeding.

The preliminary full-suite backend probe on 2026-10-25 found seven failing cases out of 1,302: one dynamic-reference scope case and six Unicode property-escape cases. The final adapter gate must retain these cases. The user requested a checkpoint after T10; runtime ownership, I/O and lifecycle work may be prepared for that review while schema qualification remains blocked. This does not authorize tool migration or a conformance claim. A backend/regex decision that resolves these failures remains necessary before T11 can pass its gate.
