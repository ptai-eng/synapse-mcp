# Third-party components

The [MIT license](../LICENSE) covers Synapse code. Dependencies retain their own licenses and notices. This inventory describes the M1 source/build contract at `1fa654be73d582e9ae4778da0d757e9112a6bac1`; it does not relicense dependencies or publish a binary distribution.

## Core and transitive headers

| Component | Version / source identity | License and local source record |
|---|---|---|
| nlohmann/json | 3.11.3, checked-in headers | MIT; copyright/SPDX notices in [json.hpp](../include/nlohmann/json.hpp). CMake uses this one source rather than downloading a second copy. |
| jsoncons | v1.10.0, `9e57e421285422d7aeeb5aa7dc839f6a19121ee6` | BSL-1.0; fetched upstream `LICENSE`. [Dependency setup](../cmake/JsonconsDependency.cmake), [correction driver](../cmake/ApplyJsonconsPatch.cmake). |
| SRELL | 2026.07, `f57d6162c7c22844727a1fb42c1d15abe0dfb0d7` from `upa-url/srell` | BSD-2-Clause; fetched `srell-src/license.txt` and header notices. Generated Unicode data carries Unicode copyright/terms notices; retain those too. [Dependency setup](../cmake/SrellDependency.cmake), [qualification](../cmake/QualifySrell.cmake). |

Upstream license sources: [nlohmann/json MIT](https://github.com/nlohmann/json/blob/v3.11.3/LICENSE.MIT), [jsoncons BSL-1.0](https://github.com/danielaparker/jsoncons/blob/9e57e421285422d7aeeb5aa7dc839f6a19121ee6/LICENSE), [SRELL BSD-2-Clause](https://github.com/upa-url/srell/blob/f57d6162c7c22844727a1fb42c1d15abe0dfb0d7/srell-src/license.txt).

For source distributions, retain applicable copyright, permission, conditions and disclaimer text. SRELL's BSD notice also requires reproduction in documentation/materials accompanying binary distributions. Preserve generated Unicode notices and follow their linked terms. A link in this inventory does not replace license text required in a distribution. CMake downloads dependencies under its build tree; this repository does not provide an install/export packaging target that assembles their notices.

## Qualified jsoncons correction

[ADR-003](decisions/0003-schema-validation.md) and its superseding [ADR-004](decisions/0004-schema-qualification-corrections.md) record the defect, qualification and removal condition. The current correction touches seven upstream headers in fetched build-tree sources. It preserves embedded-NUL string/key lengths, routes regex evaluation through SRELL's UTF-8 Unicode engine, preserves dynamic-reference resource scope and query-bearing resource/dialect identity. It leaves tracked vendor headers and upstream notices intact.

The driver verifies the exact Git source and frozen whole-file preimages/postimages. It accepts documented complete prior correction states for migration, reapplies only to known content and rejects unknown/partial edits. Reconfiguration rechecks provenance. No archive checksum is asserted for the Git fetch. The build also checks SRELL source/header identity and rejects unqualified parent targets/source overrides.

The mandatory JSON Schema 2020-12 qualification covers 46 files, 384 groups and 1,302 cases with zero skips. Hosted Windows/Linux results appear in the [verification record](verification/m1.md). Optional format assertions remain outside the default profile. Remove these corrections only after a pinned upstream replacement passes the same gates; an upstream version bump alone does not establish equivalence.

## Development-only components

| Component | Pin | License / role |
|---|---|---|
| Catch2 | 3.4.0 release archive | BSL-1.0, upstream `LICENSE.txt`; C++ tests. [Test setup](../tests/CMakeLists.txt). |
| JSON Schema Test Suite | `89e83a74014617ffbe79a56d83d14d054cd11417` | MIT, upstream `LICENSE`; test data and offline remote fixtures. [Suite setup](../cmake/SchemaTestSuite.cmake). |
| Node.js | 24.22.1 | Node's release `LICENSE` contains its MIT terms and bundled third-party notices; verifier runtime, outside the C++ target. |
| `@modelcontextprotocol/client` | 2.4.0 | Package metadata says Apache-2.0; its shipped `LICENSE` records the project's Apache-2.0/MIT transition and documentation CC-BY-4.0 terms. Preserve the shipped file. |
| `@modelcontextprotocol/inspector` | 2.12.0 | Package metadata says `SEE LICENSE IN LICENSE`; its shipped file records the same licensing transition. Named-host preparation, still unqualified. |

The npm [manifest](../tests/interop/package.json) and [lockfile](../tests/interop/package-lock.json) pin the development dependency graph. Use `npm ci --ignore-scripts`. The direct-package rows do not replace the licenses of transitive npm packages; preserve their shipped notices if redistributing those tools. Inspector 2.12.0 bundles client 2.3.1 lineage, while the harness imports client 2.4.0. Neither tool is a runtime dependency of an ordinary C++ consumer.
