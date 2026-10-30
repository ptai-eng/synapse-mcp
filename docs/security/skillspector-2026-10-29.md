# SkillSpector scan and finding review — 2026-10-29

**Scanner result: 85/100, CRITICAL, DO_NOT_INSTALL.** This is the unchanged automated assessment of source `184e406c75db840f67b7e2d250408ed133d71ce1`. Review found 199 false-positive matches for the alleged behavior and one real CI isolation limitation. The latter remains open; this report does not turn the scan into a security certification.

## Source, scanner and coverage

- Source: a `git archive` ZIP of the complete tracked tree at the SHA above, before this report and README update. It contains 97 files, including vendored JSON, CI, tests, dependency lockfile and documentation. Generated builds, installed development dependencies and untracked personal analysis are outside this source snapshot.
- Scanner: SkillSpector 2.3.11, local clean scanner checkout `c2d09df019e358d3dc12d980b82c798b87cb9f56`, Python 3.12, Windows. Static scan with `--no-llm`, no custom YARA rules, no baseline and zero suppressed findings.
- Scanner inventory: 95/95 selected components. Its built-in hidden-file rule omits `.gitignore` and `tests/interop/.node-version`; these were checked separately (ignore rules and the Node version pin). The inventory percentage is not semantic C++ coverage. C++ and `.mjs` files are labeled `other`, and `has_executable_scripts: false` must not be interpreted as absence of executable code.
- Original analysis: 202 matches before heuristic filtering, 200 retained report entries, `is_complete: false`, LLM analysis skipped. The log reports 14 internal deduplication groups; the JSON still lists all 200 retained entries. Counts below refer to those entries, not 200 independent vulnerabilities.
- The analyzer log reports no MCP manifest, so manifest-specific least-privilege/tool-poisoning checks were skipped. Five YARA rule files compiled, with no YARA findings. OSV lookups reported no known vulnerabilities for the two direct verifier packages, client 2.4.0 and Inspector 2.12.0, at scan time. This does not cover every transitive npm package or C++ dependency.

Scanning the full local worktree twice did not finish within the bounded observation period; both owned scan process chains were stopped. Inspection of the scanner's file walker showed recursive traversal before skip filtering, with no exclusion for `build/`. The Git archive scan completed and exited 1 because of its risk score, with a valid report; this exit is not a scanner crash. No rule was disabled to obtain that result.

## Automated severities and reviewed disposition

| Automated severity | Retained matches |
|---|---:|
| Critical | 0 |
| High | 55 |
| Medium | 143 |
| Low | 2 |

SkillSpector aggregates confidence-weighted severity contributions with diminishing returns per rule. Several High/Medium rule families can produce an overall Critical score without an individual Critical finding. We preserve that score rather than publishing an unofficial adjusted score.

| Rule | Matches | Location and reviewed evidence | Disposition |
|---|---:|---|---|
| PE5, High | 1 | `.github/workflows/ci.yml:31` really selects `--security-opt seccomp=unconfined` for `matrix.sanitizer == 'thread'`. | **Confirmed CI isolation limitation; retained, open hardening item.** |
| PE3, High | 52 | `tests/interop/package-lock.json`, lines 139–456: every match is the substring `keyring` in Inspector's dependency metadata/package names and URLs. Inspector 2.12.0 depends on `@napi-rs/keyring`; the file records versions, integrity hashes and platform packages. It does not read a credential file. | False positives for alleged credential access at these locations. This is not a security audit of the keyring implementation. |
| TM1, High | 2 | `cmake/TestDependencyProvenance.cmake:47,105`: regex `--no-?(?:verify\|check\|validate\|confirm\|protect\|safe)` matches the prefix of `git clone --no-checkout`. The fixture then checks out the commit and runs the provenance checker, including intentional tamper rejection. | False positives: delaying checkout does not disable certificate, authentication or provenance checks. |
| MP2, Medium | 142 | `include/nlohmann/json.hpp` (136), `include/synapse/detail/stdio_posix.hpp` (3), `include/synapse/detail/stdio_windows.hpp` (2), `include/synapse/transport.hpp` (1). Matches are indentation/macro continuation spacing, UTF-8 lookup-table repetitions, and the comment `fill the buffer` at JSON line 18747. | False positives for context-window stuffing; these locations are ordinary C++ source, not instructions to fill an agent's context. |
| EA2, Medium | 1 | `include/nlohmann/json.hpp:19685`: `auto deleter = ...` is matched as `auto delete`. The lambda deallocates one C++ object through allocator traits. | False positive for autonomous destructive decisions. |
| EA3, Low | 2 | `LICENSE:16` contains the MIT warranty phrase `NOT LIMITED TO`; `tasks/execution.md:156` records an authorized historical audit-scope extension. | False positives for an agent autonomously expanding its permissions. |

The full snippets, original severities, rule IDs and line numbers remain in the [machine report](evidence/skillspector-2026-10-29.json). The table covers all 200 retained entries. A false-positive decision is scoped to the behavior alleged at the matched location, not a claim that the surrounding component has no other defect.

## Confirmed CI limitation: PE5

The [workflow](../../.github/workflows/ci.yml) relaxes seccomp for the Linux TSan job so its child-scoped `setarch x86_64 -R` can use `personality(2)`. Disabling seccomp removes a container syscall filter; it is a real reduction in defense in depth. The finding does not by itself demonstrate a container escape or malicious SDK behavior.

The existing boundaries are a GitHub-hosted `ubuntu-24.04` job, a digest-pinned compiler image, `contents: read`, checkout with `persist-credentials: false`, and a condition limited to the `thread` sanitizer row. The YAML contains no user-supplied secrets or deployment credentials. Functional and ASan/UBSan rows do not get this option. These boundaries limit exposure but do not restore seccomp filtering, including when CI executes pull-request code.

Follow-up: evaluate a narrower seccomp configuration that allows the required personality operation, or a TSan runtime arrangement that works with the default filter. Validate the actual hosted sanitizer suite before replacing the current setting. This review documents the existing trade-off; it neither changes CI nor grants a new risk waiver. The option is not required by applications embedding Synapse and should not be copied to production.

## Reproduction and evidence integrity

With the scanner version and commit above installed, run from a Synapse checkout in PowerShell. Use a new output directory for a repeat; the example creates review artifacts only.

```powershell
$scanDir = Join-Path ([IO.Path]::GetTempPath()) ('synapse-scan-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $scanDir | Out-Null
git archive --format=zip --output="$scanDir/source.zip" 184e406c75db840f67b7e2d250408ed133d71ce1
if ($LASTEXITCODE -ne 0) { throw 'Source archive failed' }
skillspector scan "$scanDir/source.zip" --no-llm --verbose --format json --output "$scanDir/report.json"
# Exit 1 with a valid report is expected for the recorded high aggregate score.
```

Original run used `uv --project <local-SkillSpector-checkout> run --python 3.12 skillspector` as the launcher. OSV data and scanner changes can change future results; reproduce the pinned source/scanner and compare findings, not just scores.

- Source ZIP SHA-256: `349f859309636b037a8e6075be4d0551c8cad111dc3786d67e732cfca2be83e9`.
- Original local JSON SHA-256: `b2ca9b273f489c906840e8f3f7fa107324a6b8b532d8d6039187e1908348048b`.
- The retained public JSON changes only `skill.source` from a temporary absolute extraction path to `git-archive:184e406c75db840f67b7e2d250408ed133d71ce1`. All finding objects, severities, score and completeness fields are unchanged. Serialization/line endings differ, so the original JSON hash is not the public file's hash.
- Original JSON, verbose log and source ZIP remain in ignored `build/execution/recheck-20261029-*`. No suppression file was created.

Runtime test results are recorded separately in the [M1 recheck](../verification/recheck-2026-10-29.md). Passing those tests complements source review; it does not cancel a scanner finding or prove absence of all vulnerabilities.
