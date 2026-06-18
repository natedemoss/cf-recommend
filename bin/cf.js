#!/usr/bin/env node
'use strict';

// Launcher for the cf-recommend CLI. The actual program is a prebuilt native
// binary (no compiler needed to install). This script picks the binary that
// matches the current OS/architecture and forwards all arguments to it.

const path = require('path');
const fs = require('fs');
const { spawnSync } = require('child_process');

const platform = process.platform; // 'win32' | 'linux' | 'darwin' | ...
const arch = process.arch; // 'x64' | 'arm64' | ...

const exeName = platform === 'win32' ? 'cf.exe' : 'cf';
const binPath = path.join(__dirname, '..', 'binaries', `${platform}-${arch}`, exeName);

if (!fs.existsSync(binPath)) {
  process.stderr.write(
    `cf-recommend: no prebuilt binary for ${platform}-${arch}.\n` +
      'Prebuilt targets: win32-x64, linux-x64, darwin-x64, darwin-arm64.\n' +
      'To build from source, see https://github.com/nathandemoss/cf-recommend#build\n'
  );
  process.exit(1);
}

const result = spawnSync(binPath, process.argv.slice(2), { stdio: 'inherit' });

if (result.error) {
  if (result.error.code === 'EACCES') {
    process.stderr.write(
      `cf-recommend: binary is not executable: ${binPath}\n` +
        'Try reinstalling the package.\n'
    );
  } else {
    process.stderr.write(`cf-recommend: failed to launch binary: ${result.error.message}\n`);
  }
  process.exit(1);
}

// Mirror the child's exit: forward its signal or status code.
if (result.signal) {
  process.kill(process.pid, result.signal);
} else {
  process.exit(result.status === null ? 1 : result.status);
}
