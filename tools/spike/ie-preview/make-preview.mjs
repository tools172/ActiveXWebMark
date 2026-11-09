// Builds a Markdown file that mixes inline SVG and file-based SVG so the
// existing IE11 preview can be used to verify both display paths.

import fs from 'node:fs';
import path from 'node:path';

const root = path.resolve(import.meta.dirname, '..');
const outDir = path.join(root, '..', 'mermaid-sidecar', 'out');
const previewDir = path.resolve(import.meta.dirname);
fs.mkdirSync(previewDir, { recursive: true });

const flow = fs.readFileSync(path.join(outDir, 'flow.svg'), 'utf8');
const complex = fs.readFileSync(path.join(outDir, 'complex.svg'), 'utf8');

const fileUrl = (name) => {
  const absolute = path.join(outDir, name).replaceAll('\\', '/');
  return `file:///${absolute.replace(/^([A-Za-z]):/, '$1:')}`;
};

const markdown = `# Mermaid 外挂验证

IE11 只负责显示，mermaid.js 在后台的 Edge 里运行。

## 1. 内联 SVG（默认方案）

${flow}

## 2. SVG 文件 + img

<img src="${fileUrl('flow.svg')}" width="420" alt="flow">

## 3. 复杂图（内联 SVG）

${complex}

## 4. 原始 Mermaid 源码（仅作对照）

\`\`\`mermaid
graph TD
  A[开始] -->|条件| B{判断}
  B -->|是| C[执行]
  B -->|否| D[结束]
  subgraph 子流程
    C --> E[记录]
  end
\`\`\`
`;

const target = path.join(previewDir, 'spike-preview.md');
fs.writeFileSync(target, markdown, 'utf8');
console.log(target);
