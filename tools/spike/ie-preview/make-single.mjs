// Writes single-purpose preview files so each case can be verified alone.
//
//   node make-single.mjs complex   -> complex-only.md (inline SVG)
//   node make-single.mjs img       -> img-only.md (file SVG via <img>)

import fs from 'node:fs';
import path from 'node:path';

const root = path.resolve(import.meta.dirname, '..');
const outDir = path.join(root, '..', 'mermaid-sidecar', 'out');
const previewDir = path.resolve(import.meta.dirname);

const kind = process.argv[2] || 'complex';

function fileUrl(name) {
  const absolute = path.join(outDir, name).replaceAll('\\', '/');
  return `file:///${absolute}`;
}

if (kind === 'complex') {
  const svg = fs.readFileSync(path.join(outDir, 'complex.svg'), 'utf8');
  const markdown = `# 复杂图（内联 SVG）

${svg}
`;
  fs.writeFileSync(path.join(previewDir, 'complex-only.md'), markdown, 'utf8');
  console.log(path.join(previewDir, 'complex-only.md'));
} else if (kind === 'img') {
  let svg = fs.readFileSync(path.join(outDir, 'flow.svg'), 'utf8');
  // An SVG loaded through <img> needs an intrinsic size. Mermaid emits
  // width="100%" plus a max-width style, which IE11 lays out poorly as an image.
  svg = svg
    .replace('width="100%"', 'width="110" height="168"')
    .replace(/style="max-width: [^"]*;"/, '');
  const fixed = path.join(outDir, 'flow-img.svg');
  fs.writeFileSync(fixed, svg, 'utf8');
  const markdown = `# SVG 文件 + img

<img src="${fileUrl('flow-img.svg')}" width="420" alt="flow">
`;
  fs.writeFileSync(path.join(previewDir, 'img-only.md'), markdown, 'utf8');
  console.log(path.join(previewDir, 'img-only.md'));
} else {
  console.error('usage: node make-single.mjs complex|img');
  process.exit(1);
}
