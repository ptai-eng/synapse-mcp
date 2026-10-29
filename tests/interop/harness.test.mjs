import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {mkdtempSync, readFileSync, writeFileSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join, dirname} from 'node:path';
import {fileURLToPath} from 'node:url';

// These are verifier regressions, independent of the native server's unit cases.
const [server, compiler, environment, source, git, group = 'all'] = process.argv.slice(2);
const scratch = mkdtempSync(join(tmpdir(), 'synapse-interop-'));
const output = join(scratch, 'evidence.json');
const harness = process.env.SYNAPSE_INTEROP_TEST_HARNESS || join(dirname(fileURLToPath(import.meta.url)), 'run-client.mjs');
const run = (binary = server, env = {}, provenance = git) => spawnSync(process.execPath,
  [harness, binary, compiler, environment, source, output, provenance],
  {encoding:'utf8', timeout:20000, env:{...process.env, ...env}});
const evidence = () => JSON.parse(readFileSync(output, 'utf8'));
let result;
if (group === 'all' || group === 'evidence') {
result = run();
assert.equal(result.status, 0, result.stderr);
const first = evidence();
assert.equal(first.status, 'passed');
assert.match(first.evidence.binarySHA256, /^[a-f0-9]{64}$/);
if (process.platform === 'win32') assert.match(first.evidence.standardLibrary, /_MSVC_STL_VERSION=\d+; _MSVC_STL_UPDATE=\d+/);
else assert.match(first.evidence.standardLibrary, /(__GLIBCXX__|_LIBCPP_VERSION)=\d+/);
result = run(join(scratch, 'missing-server'));
assert.notEqual(result.status, 0);
assert.equal(evidence().status, 'failed');
assert.notEqual(evidence().runId, first.runId);
for (const fixture of ['--wrong-revision', '--failed-operation']) {
  result = run(fixture);
  assert.notEqual(result.status, 0);
  assert.equal(evidence().status, 'failed');
  assert.ok(evidence().transcript.length > 0);
}
result = run(server, {SYNAPSE_INTEROP_TEST_CLEANUP_FAILURE:'1'});
assert.notEqual(result.status, 0);
assert.equal(evidence().status, 'failed');
assert.match(evidence().cleanupError, /fixture cleanup failure/);
}
if (group === 'all' || group === 'provenance') {
// A changed checkout after configuration must be read anew at execution.
const fakeGit = join(scratch, 'git.mjs');
const revisionFile = join(scratch, 'revision');
writeFileSync(fakeGit, `import {readFileSync} from 'node:fs'; console.log(process.argv.includes('rev-parse') ? readFileSync(${JSON.stringify(revisionFile)}, 'utf8') : ' M changed.cpp');`);
for (const revision of ['configured-commit', 'changed-after-configure']) {
  writeFileSync(revisionFile, revision);
  result = run(server, {SYNAPSE_INTEROP_TEST_GIT_SCRIPT:fakeGit}, process.execPath);
  assert.equal(result.status, 0, result.stderr);
  assert.equal(evidence().evidence.runtimeCheckout.commit, revision);
  assert.equal(evidence().evidence.runtimeCheckout.dirty, true);
  assert.match(evidence().evidence.buildSource, /unknown/);
}
result = run(server, {}, join(scratch, 'missing-git'));
assert.notEqual(result.status, 0);
assert.equal(evidence().status, 'failed');
}
if (group === 'all' || group === 'cleanup') {
const pidFile = join(scratch, 'pid');
result = run('--failed-operation', {SYNAPSE_INTEROP_TEST_STUBBORN:'1', SYNAPSE_INTEROP_TEST_DEADLINE_ON_CLOSE:'1', SYNAPSE_INTEROP_TEST_PID_FILE:pidFile});
assert.notEqual(result.status, 0);
assert.equal(evidence().status, 'failed');
assert.match(evidence().error, /Overall timeout/);
const pid = Number(readFileSync(pidFile, 'utf8'));
try {
  assert.equal(evidence().deadlineCleanup.transportPid, null, 'deadline must intersect SDK close after its PID getter is cleared');
  assert.equal(evidence().deadlineCleanup.signalSent, true, 'deadline must explicitly terminate the still-live child');
  assert.equal(evidence().deadlineCleanup.reaped, true, 'deadline must await actual transport close before parent exit');
  await new Promise(resolve => setTimeout(resolve, 250));
  assert.throws(() => process.kill(pid, 0), /ESRCH/);
} finally {
  // A failing mutation/control must not itself leave the fixture behind.
  try { process.kill(pid, 'SIGKILL'); } catch {}
}
}
console.log(`Interop ${group} regressions passed`);
