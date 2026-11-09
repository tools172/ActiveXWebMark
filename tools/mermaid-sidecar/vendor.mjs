// Copies the built sidecar next to the other vendored runtime assets.
//
// Build happens first (`npm run vendor` does both), then the app's CMake build
// copies third_party/mermaid/ beside the executable.

import fs from 'node:fs';
import path from 'node:path';

const root = process.cwd();
const target = path.resolve(root, '..', '..', 'third_party', 'mermaid');
fs.mkdirSync(target, { recursive: true });

for (const file of ['sidecar.cjs', 'mermaid.min.js']) {
  fs.copyFileSync(path.join(root, 'dist', file), path.join(target, file));
}
console.log(`vendored -> ${target}`);
