#!/usr/bin/env node
// Generate a local Inspector 2.12.0 read-only session, never a global catalog.
import { access, mkdir, stat, writeFile } from 'node:fs/promises';
import { constants } from 'node:fs';
import { dirname, isAbsolute, resolve } from 'node:path';

const usage = 'Usage: node tests/interop/inspector-config.mjs --binary <absolute-native_status-path> --output <new-config-path> [-- <server-args...>]';

async function main(argv) {
  let binary;
  let output;
  let args = [];
  for (let index = 0; index < argv.length; ++index) {
    const option = argv[index];
    if (option === '--') {
      args = argv.slice(index + 1);
      break;
    }
    if (option !== '--binary' && option !== '--output') throw new Error(usage);
    const value = argv[++index];
    if (!value || value.startsWith('--')) throw new Error(usage);
    if (option === '--binary') {
      if (binary !== undefined) throw new Error('Duplicate --binary');
      binary = value;
    } else {
      if (output !== undefined) throw new Error('Duplicate --output');
      output = value;
    }
  }
  if (!binary || !output) throw new Error(usage);
  if (!isAbsolute(binary)) throw new Error('--binary must be an absolute path');
  if (!(await stat(binary)).isFile()) throw new Error('--binary must name an existing file');
  await access(binary, constants.X_OK);
  const destination = resolve(output);
  if (destination === resolve(binary)) throw new Error('Config must not replace the binary');
  const config = {
    mcpServers: {
      native_status: {
        type: 'stdio',
        command: binary,
        args,
        protocolEra: 'legacy',
        requestTimeout: 5000,
        connectionTimeout: 5000,
      },
    },
  };
  await mkdir(dirname(destination), { recursive: true });
  // Exclusive creation also protects existing session files and symlink targets.
  await writeFile(destination, `${JSON.stringify(config, null, 2)}\n`, { flag: 'wx', mode: 0o600 });
  console.log(destination);
}

main(process.argv.slice(2)).catch((error) => {
  console.error(`Inspector config: ${error.message}`);
  process.exitCode = 1;
});
