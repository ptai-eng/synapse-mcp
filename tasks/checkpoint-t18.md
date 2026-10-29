# T18 raw-process checkpoint

Completed: 2026-10-29. Source/test commit: `1aa2ecc`. Commits remain local.

T18 is implemented and independently reviewed. Seven raw-child cases verify malformed-envelope recovery, idle/cooperative EOF, explicit stop with stdin still open, blocked stdout, warmed native handle/FD cleanup, and a gated noncooperative callback. These cases launch native binaries directly; SDK close escalation is not used as graceful-shutdown proof.

Each graceful case requires the child to be alive before the trigger, then exit successfully within two seconds measured after EOF/stop/release delivery. Forced termination is failure cleanup. The fixture's callback state outlives the joined server, including assertion/error unwinding.

Seven deliberately broken controls returned nonzero. Additional early-success-exit controls reject a child that terminated before the requested shutdown. Post-review final sources passed 7/7 on Windows/MSVC and 7/7 on Ubuntu WSL/GCC. Earlier source iterations also passed 70 repeated executions per platform; those repeats are not misattributed to the final lifetime/oracle correction. Windows process plus official-client selection passed 11/11 before the scoped review corrections; the final process subset was rerun afterward.

Independent review identified the fixture lifetime and pre-trigger exit oracle defects; both were fixed and rereviewed with spec and quality APPROVE. No core SDK change was needed. Local evidence: `build/execution/T18-report.md`, `T18-review.md`, `T18-fix-review.md`, and platform/negative-control logs.

Re-run after building `synapse_process_server` and `native_status` with interop enabled:

```sh
ctest --test-dir <configured-build> --output-on-failure -R '^synapse\.M1\.process\.'
```

Use `-C Debug` for the MSVC multi-config tree. These local results do not establish the hosted CI matrix or a named-host walkthrough. T19 requires actual hosted results; T20-T21 are deferred at the user's request.
