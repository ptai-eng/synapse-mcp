# T21 supporting-documentation checkpoint

Update 2026-10-30: the user authorized README completion on 2026-10-29. The final README reference/consumer/extracted sample and official-client flow passed from a fresh pinned checkout; see [README reproduction](../docs/verification/readme-2026-10-29.md). AC-08 is satisfied for the documented Windows flow. The earlier deferral below is historical. Release/version selection and publication remain separate work.

Date: 2026-10-29. Status: the user-authorized non-README slice is complete and independently approved. The latest user scope excludes README; all authorized T21 supporting-documentation work is complete. Full original M1 acceptance still includes deferred README/AC-08.

## Delivered scope

- [Migration guide](../docs/migration/v0.2-to-m1.md): typed API changes, errors and policy boundaries, borrowed-state lifetime and cooperative shutdown.
- [Third-party inventory](../docs/third-party.md): exact pins, qualified schema corrections, licenses and remaining distribution-notice obligations.
- [Verification record](../docs/verification/m1.md): measured CI and official-client results, reproducible commands and explicit evidence limitations.
- [Specification status](../specs/001-native-stdio-foundation/spec.md): implemented runtime separated from incomplete acceptance, without weakening the criteria.

## Verification and review

Fresh remote checkout at `1fa654be73d582e9ae4778da0d757e9112a6bac1`, under a path containing spaces, configured and built the reference server and downstream consumer. Consumer execution passed. Quoted Windows SDK configuration arguments were reproduced. The environment probe came from the same reference build.

The pinned official client passed against that fresh reference binary, negotiating `2025-11-25` and exercising tools, resources and prompts. The migration guide's extracted C++ example compiled successfully. Local Markdown validation checked 45 path/anchor links with zero errors; `git diff --check` passed. No fresh full regression suite or Linux documentation reproduction is claimed: runtime source is unchanged from the reviewed [T19 matrix](checkpoint-t19.md).

Independent review approved all four documents with no actionable findings, checking public headers, dependency licenses, recorded evidence and reproduction logs. Detailed local evidence remains in ignored `build/execution/T21-report.md` and `build/execution/T21-review.md`.

README SHA-256 before and after is `2E2CA4B0F092C51C9AD503988617A0D6F062DBCC0B35A743431C743E01BDDFDE`; its contents are unchanged.

## Remaining gates

T20 and AC-06 subsequently passed the actual user-operated named-host walkthrough and transcript review; see [T20](checkpoint-t20.md). The user reaffirmed README deferral after a separate draft was prepared and reproduced. That draft and its logs remain ignored under build/execution; README is restored to its committed content. README/AC-08 remains deferred. This checkpoint completes only the supporting-documentation slice and makes no release, publication or universal host-compatibility claim. The prepared T20 generator and user analysis directory remain outside this commit scope.
