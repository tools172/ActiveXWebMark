// Bundles the sidecar and copies the mermaid browser bundle next to it.
//
// Output:
//   dist/sidecar.cjs      (~2.3 MB, includes puppeteer-core)
//   dist/mermaid.min.js   (~2.7 MB, official mermaid 11.12.1 browser bundle)

import fs from 'node:fs';
import path from 'node:path';
import { build } from 'esbuild';

const root = process.cwd();
const dist = path.join(root, 'dist');
fs.mkdirSync(dist, { recursive: true });

await build({
  entryPoints: [path.join(root, 'sidecar.mjs')],
  bundle: true,
  platform: 'node',
  format: 'cjs',
  target: 'node18',
  outfile: path.join(dist, 'sidecar.cjs'),
  logLevel: 'warning',
});

const mermaidSource = path.join(root, 'node_modules', 'mermaid', 'dist', 'mermaid.min.js');
const mermaidTarget = path.join(dist, 'mermaid.min.js');
fs.copyFileSync(mermaidSource, mermaidTarget);

const kb = (file) => (fs.statSync(file).size / 1024).toFixed(0) + ' KB';
console.log(`dist/sidecar.cjs    ${kb(path.join(dist, 'sidecar.cjs'))}`);
console.log(`dist/mermaid.min.js ${kb(mermaidTarget)}`);
