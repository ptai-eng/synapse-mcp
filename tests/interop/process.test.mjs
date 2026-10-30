import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { mkdtempSync, writeFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { performance } from 'node:perf_hooks';
import { createInterface } from 'node:readline';

// Deliberately broken peer: verifies the assertions reject old/unowned behavior.
if (process.argv[2] === '--stub') {
  const group = process.argv[3];
  if (process.argv[4] === '--early-exit') {
    console.error('READY\nENTERED\nQUEUED');
    if (group === 'blocked') process.exit(0);
    process.stdout.write(JSON.stringify({jsonrpc:'2.0',id:1,result:{protocolVersion:'2025-11-25'}}) + '\n', () => process.exit(0));
  } else {
  setInterval(() => {}, 1000);
  console.error('READY\nENTERED\nQUEUED\nWAIT_INVALID\nCOUNTS 10 11');
  createInterface({input:process.stdin}).on('line', line => {
    const message = JSON.parse(line);
    if (message?.method === 'initialize') console.log(JSON.stringify({jsonrpc:'2.0',id:1,result:{protocolVersion:'2025-11-25'}}));
    if (message?.method === 'ping' && message.id === 22) console.log(JSON.stringify({jsonrpc:'2.0',id:22,result:{}}));
  });
  }
} else {
  const [fixture, reference, group, negative] = process.argv.slice(2);
  assert.equal(process.version, 'v24.22.1', 'pinned local Node is required');
  const gate = mkdtempSync(join(tmpdir(), 'synapse-process-'));
  const child = spawn(negative ? process.execPath : group === 'idle' || group === 'recover' ? reference : fixture,
    negative ? [fileURLToPath(import.meta.url), '--stub', group, negative] : group === 'idle' || group === 'recover' ? [] : [group, gate],
    { stdio:['pipe','pipe','pipe'], windowsHide:true });
  let stderr = '', stdout = '', forced = false, closed = false;
  let outcome;
  const exited = new Promise(resolve => child.once('exit', (code, signal) => { outcome = {code, signal}; resolve(outcome); }));
  const reaped = new Promise(resolve => child.once('close', () => { closed = true; resolve(); }));
  let spawnError;
  child.once('error', error => { spawnError = error; });
  child.stdin.on('error', () => {});
  child.stderr.on('data', bytes => { stderr += bytes; });
  if (group !== 'blocked') child.stdout.on('data', bytes => { stdout += bytes; });
  const watchdog = setTimeout(() => {
    forced = true; process.exitCode = 1;
    console.error(`FAIL ${group}: overall watchdog; forced child cleanup`);
    child.kill('SIGKILL'); child.stdout.resume();
  }, 12000);
  async function until(predicate, description, budget = 3000) {
    const deadline = performance.now() + budget;
    while (!predicate()) {
      if (spawnError) throw spawnError;
      assert.ok(performance.now() < deadline, `${description}: timeout; stderr=${stderr}`);
      await new Promise(resolve => setTimeout(resolve, 5));
    }
  }
  function send(value) { child.stdin.write(JSON.stringify(value) + '\n'); }
  function messages() {
    // Pipe reads may split a JSON line at any byte; inspect complete frames only.
    return stdout.slice(0, stdout.lastIndexOf('\n') + 1).split('\n').filter(Boolean).map(line => JSON.parse(line));
  }
  async function initialize() {
    send({jsonrpc:'2.0', id:1, method:'initialize', params:{protocolVersion:'2025-11-25',capabilities:{},clientInfo:{name:'raw-process',version:'1'}}});
    await until(() => stdout.includes('\n'), 'initialize response');
    assert.equal(messages()[0].result.protocolVersion, '2025-11-25');
    send({jsonrpc:'2.0',method:'notifications/initialized'});
  }
  async function graceful(trigger, description, requireAlive = true) {
    if (requireAlive) {
      assert.equal(outcome, undefined, `${description}: child exited before trigger`);
      assert.equal(child.exitCode, null, `${description}: child exited before trigger`);
      assert.equal(child.signalCode, null, `${description}: child terminated before trigger`);
    }
    // Start the budget at EOF/stop delivery, after startup/handshake/work gates.
    const started = performance.now();
    trigger();
    let timer;
    try {
      const result = await Promise.race([exited, new Promise((_, reject) => { timer = setTimeout(() => reject(new Error(`${description}: did not exit within 2000ms`)), 2000); })]);
      const elapsed = performance.now() - started;
      assert.equal(result.signal, null, `${description}: unexpected termination signal`);
      assert.equal(result.code, 0, `${description}: exit code; stderr=${stderr}`);
      assert.ok(elapsed <= 2000, `${description}: elapsed ${elapsed}ms`);
      assert.equal(forced, false, 'forced cleanup never proves graceful shutdown');
      child.stdout.resume();
      await reaped; // Settle diagnostics and every pipe before inspecting markers.
      console.log(`${description}: ${elapsed.toFixed(1)}ms, code=0, signal=null`);
    } finally { clearTimeout(timer); }
  }
  try {
    // Make the early-success control deterministic: exercise the same oracle
    // with a known settled, successful child rather than racing OS exit events.
    if (negative === '--early-exit') await exited;
    if (group === 'cleanup') {
      await until(() => /COUNTS (\d+) (\d+)\r?\n/.test(stderr), 'resource count');
      const match = /COUNTS (\d+) (\d+)/.exec(stderr);
      assert.ok(match); assert.equal(match[1], match[2], '32 joined lifecycles must return to warmed handle/FD baseline');
      await graceful(() => {}, 'resource cleanup', false);
    } else if (group === 'blocked') {
      await until(() => stderr.includes('QUEUED'), 'output admitted');
      assert.equal(child.stdout.readableFlowing, null, 'stdout must remain unread');
      assert.equal(child.stdin.writableEnded, false);
      await graceful(() => writeFileSync(join(gate, 'stop'), ''), 'blocked stdout explicit stop');
    } else {
      await initialize();
      if (group === 'recover') {
        // Syntactically valid JSON, invalid JSON-RPC envelopes, then good ping.
        for (const value of [[], 42, {jsonrpc:'2.0', id:20, method:7}, {jsonrpc:'2.0', id:21, method:'ping',params:7}]) send(value);
        send({jsonrpc:'2.0',id:22,method:'ping'});
        await until(() => messages().some(m => m.id === 22), 'working ping after malformed envelopes');
        const replies = messages().slice(1);
        assert.equal(replies.filter(m => m.error?.code === -32600).length, 4);
        assert.deepEqual(replies.find(m => m.id === 22).result, {});
      } else if (['cooperative','stop','gated'].includes(group)) {
        send({jsonrpc:'2.0',id:2,method:'tools/call',params:{name:'work',arguments:{}}});
        await until(() => stderr.includes('ENTERED'), 'callback entered');
      }
      if (group === 'gated') {
        writeFileSync(join(gate, 'stop'), '');
        await until(() => stderr.includes('WAIT_FALSE') || stderr.includes('WAIT_INVALID'), 'bounded wait observation');
        assert.ok(stderr.includes('WAIT_FALSE'), 'wait_for must remain false with live callback');
        assert.equal(outcome, undefined, 'child must remain alive while gate is held');
        assert.equal(child.stdin.writableEnded, false);
        await graceful(() => writeFileSync(join(gate, 'release'), ''), 'gate release joins owned callback');
        assert.ok(stderr.includes('RELEASED'));
      } else if (group === 'stop') {
        assert.equal(child.stdin.writableEnded, false);
        await graceful(() => writeFileSync(join(gate, 'stop'), ''), 'callback explicit stop with stdin open');
      } else {
        await graceful(() => child.stdin.end(), `${group} EOF`);
        if (group === 'cooperative') assert.ok(stderr.includes('CANCELLED'), 'EOF requests active callback cancellation');
      }
    }
  } catch (error) {
    process.exitCode = 1;
    console.error(`FAIL ${group}: ${error.stack}`);
  } finally {
    if (!outcome && !spawnError) {
      forced = true; process.exitCode = 1;
      console.error(`FAIL ${group}: forced cleanup of owned child PID ${child.pid}`);
      child.kill('SIGKILL');
    }
    // A paused stdout must drain after exit/kill so the owned child is reaped.
    child.stdout.resume(); child.stdin.destroy();
    await reaped;
    clearTimeout(watchdog);
    rmSync(gate, {recursive:true,force:true});
    assert.ok(closed, 'owned child close observed');
    if (forced) process.exitCode = 1;
  }
}
