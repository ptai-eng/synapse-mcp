import assert from 'node:assert/strict';
import { readFileSync, writeFileSync, mkdirSync, existsSync } from 'node:fs';
import { dirname, isAbsolute } from 'node:path';
import { release } from 'node:os';
import { createInterface } from 'node:readline';
import { fileURLToPath } from 'node:url';
import { execFile } from 'node:child_process';
import { createHash, randomUUID } from 'node:crypto';
import { Client } from '@modelcontextprotocol/client';
import { StdioClientTransport } from '@modelcontextprotocol/client/stdio';

const REVISION = '2025-11-25';
if (process.argv[2] === '--wrong-server' || process.argv[2] === '--failed-server') {
  if (process.env.SYNAPSE_INTEROP_TEST_STUBBORN === '1') setInterval(() => {}, 1000);
  createInterface({ input: process.stdin }).on('line', line => {
    const m = JSON.parse(line);
    if (m.method === 'initialize') console.log(JSON.stringify({jsonrpc:'2.0', id:m.id, result:{protocolVersion:process.argv[2] === '--wrong-server' ? '2025-03-26' : REVISION, capabilities:{tools:{}}, serverInfo:{name:'negative-fixture',version:'1'}}}));
    if (m.method === 'tools/list') console.log(JSON.stringify({jsonrpc:'2.0',id:m.id,error:{code:-32603,message:'fixture operation failure'}}));
  });
} else {
  const args = process.argv.slice(2);
  const wrong = args[0] === '--wrong-revision';
  const failed = args[0] === '--failed-operation';
  const command = wrong || failed ? process.execPath : args[0];
  const serverArgs = wrong || failed ? [fileURLToPath(import.meta.url), wrong ? '--wrong-server' : '--failed-server'] : [];
  const transcript = [];
  let inner;
  let client;
  let transport;
  let closePromise;
  let childPid;
  let resolveChildClosed;
  const childClosed = new Promise(resolve => { resolveChildClosed = resolve; });
  let deadlinePromise;
  const metadataChildren = new Set();
  function executeMetadata(file, parameters) {
    return new Promise((resolve, reject) => {
      const subprocess = execFile(file, parameters, {encoding:'utf8', timeout:5000, killSignal:'SIGKILL', windowsHide:true},
        (error, stdout) => error ? reject(error) : resolve(stdout));
      const entry = {subprocess};
      entry.closed = new Promise(resolve => subprocess.once('close', () => { metadataChildren.delete(entry); resolve(); }));
      metadataChildren.add(entry);
    });
  }
  const evidence = {runId:randomUUID(), status:'running', startedAt:new Date().toISOString(), evidence:{node:process.version, os:`${process.platform} ${process.arch} ${release()}`, compiler:args[1], server:command, buildSource:'unknown: binary build provenance was not captured; runtimeCheckout describes only the current checkout'}, transcript};
  function saveEvidence() {
    if (args[4]) { mkdirSync(dirname(args[4]), {recursive:true}); writeFileSync(args[4], JSON.stringify(evidence, null, 2) + '\n'); }
  }
  saveEvidence();
  async function failDeadline() {
    evidence.status = 'failed'; evidence.error = 'Overall timeout';
    process.exitCode = 1;
    evidence.deadlineCleanup = {trackedPid:childPid ?? null, transportPid:inner?.pid ?? null, signalSent:false};
    if (childPid) {
      try { process.kill(childPid, 'SIGKILL'); evidence.deadlineCleanup.signalSent = true; }
      catch (error) { evidence.deadlineCleanup.error = String(error); }
    }
    // Keep this parent alive until the killed child is reaped. Reserve one
    // second inside the 15-second overall budget for this bounded drain.
    const drains = evidence.deadlineCleanup.trackedPid ? [childClosed] : [];
    for (const entry of metadataChildren) {
      try { entry.subprocess.kill('SIGKILL'); } catch {}
      drains.push(entry.closed);
    }
    if (drains.length) {
      let drainTimer;
      try {
        evidence.deadlineCleanup.reaped = await Promise.race([
          Promise.all(drains).then(() => true),
          new Promise(resolve => { drainTimer = setTimeout(() => resolve(false), 1000); })
        ]);
      } finally { clearTimeout(drainTimer); }
    }
    evidence.finishedAt = new Date().toISOString();
    try { saveEvidence(); } catch (error) { console.error('Evidence write failure:', error); }
    console.error(evidence.error); process.exit(1);
  }
  const triggerDeadline = () => deadlinePromise ??= failDeadline();
  const watchdog = setTimeout(triggerDeadline, 14000);
  try {
    assert.ok(command && isAbsolute(command), 'absolute server binary path required');
    assert.ok(existsSync(command), 'server binary does not exist');
    evidence.evidence.binarySHA256 = createHash('sha256').update(readFileSync(command)).digest('hex');
    const pkg = JSON.parse(readFileSync(new URL('./node_modules/@modelcontextprotocol/client/package.json', import.meta.url)));
    evidence.evidence.sdk = pkg.version;
    assert.equal(pkg.version, '2.4.0');
    assert.equal(process.version, 'v24.22.1');
    inner = new StdioClientTransport({ command, args: serverArgs, stderr:'inherit', env: (wrong || failed) && process.env.SYNAPSE_INTEROP_TEST_STUBBORN === '1' ? {SYNAPSE_INTEROP_TEST_STUBBORN:'1'} : undefined });
    transport = {
      onmessage: undefined, onerror: undefined, onclose: undefined,
      start() {
        inner.onmessage = (message, extra) => { transcript.push({ direction:'server', message }); this.onmessage?.(message, extra); };
        inner.onerror = error => this.onerror?.(error);
        inner.onclose = () => { childPid = undefined; resolveChildClosed(); this.onclose?.(); };
        return inner.start().then(() => { childPid = inner.pid; if (process.env.SYNAPSE_INTEROP_TEST_PID_FILE) writeFileSync(process.env.SYNAPSE_INTEROP_TEST_PID_FILE, String(childPid)); });
      },
      send(message, options) { transcript.push({ direction:'client', message }); return inner.send(message, options); },
      close() {
        if (!closePromise) {
          closePromise = inner.close();
          // Deterministically exercise the real deadline handler only after
          // SDK.close has cleared its PID getter, independently of startup speed.
          if (failed && process.env.SYNAPSE_INTEROP_TEST_DEADLINE_ON_CLOSE === '1') queueMicrotask(triggerDeadline);
        }
        return closePromise;
      },
      setProtocolVersion(version) { return inner.setProtocolVersion?.(version); }
    };
    client = new Client({name:'synapse-interop',version:'1.0.0'}, {versionNegotiation:{mode:'legacy'}, supportedProtocolVersions:[REVISION]});
    const opts = {timeout:5000};
    await client.connect(transport, opts);
    const init = transcript.find(e => e.direction === 'client' && e.message.method === 'initialize');
    assert.equal(init?.message.params.protocolVersion, REVISION);
    const reply = transcript.find(e => e.direction === 'server' && e.message.id === init.message.id);
    assert.equal(reply?.message.result?.protocolVersion, REVISION);
    assert.ok(transcript.some(e => e.direction === 'client' && e.message.method === 'notifications/initialized'));
    const tools = await client.listTools({}, opts);
    assert.ok(tools.tools.some(t => t.name === 'app.status'));
    const result = await client.callTool({name:'app.status',arguments:{}}, opts);
    assert.notEqual(result.isError, true);
    const state = JSON.parse(result.content.find(c => c.type === 'text').text);
    assert.equal(state.handled_operations, 1);
    assert.equal(state.counter_scope, 'app.status tool invocations');
    assert.ok(Number.isInteger(state.uptime_ms) && state.uptime_ms >= 0);
    const resources = await client.listResources({}, opts);
    assert.ok(resources.resources.some(r => r.uri === 'app://status'));
    const resource = await client.readResource({uri:'app://status'}, opts);
    const snapshot = JSON.parse(resource.contents[0].text);
    assert.equal(snapshot.handled_operations, state.handled_operations);
    assert.equal(snapshot.counter_scope, state.counter_scope);
    assert.ok(snapshot.uptime_ms >= state.uptime_ms);
    const prompts = await client.listPrompts({}, opts);
    assert.ok(prompts.prompts.some(p => p.name === 'explain_status'));
    const prompt = await client.getPrompt({name:'explain_status',arguments:{}}, opts);
    assert.equal(prompt.messages[0].role, 'user');
    assert.match(prompt.messages[0].content.text, /^Explain this application status: /);
    const promptState = JSON.parse(prompt.messages[0].content.text.slice('Explain this application status: '.length));
    assert.equal(promptState.handled_operations, state.handled_operations);
    assert.equal(promptState.counter_scope, state.counter_scope);
    assert.ok(promptState.uptime_ms >= snapshot.uptime_ms);
    assert.ok(args[1] && args[2] && args[3] && args[5], 'compiler, standard library, source directory and Git executable required');
    const git = async (...gitArgs) => {
      const prefix = process.env.SYNAPSE_INTEROP_TEST_GIT_SCRIPT ? [process.env.SYNAPSE_INTEROP_TEST_GIT_SCRIPT] : [];
      // Async subprocesses keep the overall watchdog effective during metadata
      // reads; a stalled Git/probe cannot block the deadline's cleanup handler.
      return (await executeMetadata(args[5], [...prefix, '-C', args[3], ...gitArgs])).trim();
    };
    evidence.evidence.runtimeCheckout = {commit:await git('rev-parse', 'HEAD'), dirty:!!(await git('status', '--porcelain'))};
    assert.ok(isAbsolute(args[2]), 'absolute environment probe path required');
    evidence.evidence.standardLibrary = (await executeMetadata(args[2], [])).trim();
    assert.match(evidence.evidence.standardLibrary, /^(MSVC STL _MSVC_STL_VERSION=\d+; _MSVC_STL_UPDATE=\d+|libstdc\+\+ __GLIBCXX__=\d+|libc\+\+ _LIBCPP_VERSION=\d+)$/);
    evidence.evidence.negotiatedRevision = reply.message.result.protocolVersion;
    // Success is finalized only once required cleanup has also completed.
  } catch (error) {
    console.error('Interop failure:', error.stack || error);
    evidence.status = 'failed'; evidence.error = String(error);
    process.exitCode = 1;
  } finally {
    if (inner) {
      try {
        await transport.close();
        if (deadlinePromise) await deadlinePromise;
        if (process.env.SYNAPSE_INTEROP_TEST_CLEANUP_FAILURE === '1') throw new Error('fixture cleanup failure');
      } catch (error) {
        console.error('Cleanup failure:', error);
        evidence.status = 'failed'; evidence.cleanupError = String(error); process.exitCode = 1;
      }
    }
    evidence.finishedAt = new Date().toISOString();
    if (evidence.status === 'running') evidence.status = 'passed';
    try { saveEvidence(); } catch (error) { console.error('Evidence write failure:', error); process.exitCode = 1; }
    console.log(JSON.stringify(evidence, null, 2));
    clearTimeout(watchdog);
  }
}
