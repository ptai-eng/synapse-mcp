# T19 CI implementation readiness

Date: 2026-10-29. Status: implementation and local checks ready; independent review interrupted by the account usage limit, hosted qualification pending. T19/AC-07 is not complete.

The workflow now targets master and codex/** pushes, master pull requests and manual dispatch. It pins released GitHub actions by full SHA, Node 24.22.1 and the planned GCC 14.4.0 container digest. Separate Linux functional, ASan+UBSan and TSan rows accompany the Windows 2022 VS 2022/toolset 14.44/SDK 26100.0 row. Native binaries and Node verification run in the same Linux container.

Local verification includes actionlint, extracted Bash syntax, CMake include-hook configuration, invalid/unsupported sanitizer rejection, actual compiled STL metadata, missing-probe rejection and observed CTest gate controls. Each unit/schema/consumer/interop failure sentinel returned nonzero; the passing control succeeded. These local controls do not establish a hosted job conclusion.

The sanitizer module fails configuration when a requested runtime cannot execute. ASan+UBSan and TSan use separate configurations. TSan's ASLR workaround is scoped to child commands in its CI container; no host sysctl is modified. CI retains allowlisted logs and environment evidence on failure.

## Remaining gate

No hosted matrix or sanitizer run has occurred. Docker remains stopped. Remote verification needs authorization to publish the feature branch; no master update, merge or release is part of that publication. Once authorized:

1. Publish codex/product-protocol-foundation so its codex/** push triggers the workflow.
2. Record actual functional Windows/Linux and sanitizer outcomes, toolchain/runner versions, run URLs and artifacts. A skipped or unavailable sanitizer is not success.
3. Where GitHub permits manual dispatch on that workflow/ref, run unit/schema/consumer/interop failure_probe controls and require failed hosted conclusions, then a normal none run that passes.
4. Resolve genuine failures and rereview corrections before checking T19 GREEN. Do not change master merely to enable dispatch.

Local implementation details and commands: ignored build/execution/T19-report.md, T19-preparation.md and T19-review.md. T20-T21 are deferred at the user's request. README is unchanged.
