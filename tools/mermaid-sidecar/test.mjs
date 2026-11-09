// End-to-end spike test for the diagram sidecar.
//
// Checks:
//   1. sidecar startup (should be fast; Edge is lazy)
//   2. first Mermaid render (includes Edge startup), then warm renders
//   3. PlantUML via remote Kroki
//   4. output SVG structure (no <foreignObject> for Mermaid)
//   5. parse error handling
//   6. Edge runs headless (no visible main window)
//   7. idle shutdown

import fs from 'node:fs';
import path from 'node:path';
import { spawn, execFileSync } from 'node:child_process';
import { once } from 'node:events';

const root = process.cwd();
const outDir = path.join(root, 'out');
fs.mkdirSync(outDir, { recursive: true });

const sidecar = path.join(root, 'dist', 'sidecar.cjs');
const mermaid = path.join(root, 'dist', 'mermaid.min.js');
if (!fs.existsSync(sidecar) || !fs.existsSync(mermaid)) {
  console.error('Build first: npm run build');
  process.exit(1);
}

const child = spawn(
  process.execPath,
  [sidecar, '--idle-ms', '20000', '--mermaid', mermaid],
  { windowsHide: true, stdio: ['pipe', 'pipe', 'pipe'] },
);

const ready = { promise: null, resolve: null, info: null };
ready.promise = new Promise((resolve) => { ready.resolve = resolve; });
const browserReady = { promise: null, resolve: null, info: null };
browserReady.promise = new Promise((resolve) => { browserReady.resolve = resolve; });

const pending = new Map();
let stderrText = '';

function readLines(stream, onLine) {
  let buffer = '';
  stream.setEncoding('utf8');
  stream.on('data', (chunk) => {
    buffer += chunk;
    let index;
    while ((index = buffer.indexOf('\n')) >= 0) {
      const line = buffer.slice(0, index).trim();
      buffer = buffer.slice(index + 1);
      if (line) {
        onLine(line);
      }
    }
  });
}

readLines(child.stdout, (line) => {
  let message;
  try {
    message = JSON.parse(line);
  } catch {
    return;
  }
  const waiter = pending.get(message.id);
  if (waiter) {
    pending.delete(message.id);
    clearTimeout(waiter.timer);
    waiter.resolve(message);
  }
});

readLines(child.stderr, (line) => {
  stderrText += line + '\n';
  try {
    const message = JSON.parse(line);
    if (message.event === 'ready') {
      ready.info = message;
      ready.resolve(message);
    } else if (message.event === 'browser-ready') {
      browserReady.info = message;
      browserReady.resolve(message);
    }
  } catch {
    // Non-JSON stderr is kept for diagnostics.
  }
});

child.on('error', (error) => {
  console.error('sidecar spawn error:', error);
});

function request(id, options, timeoutMs = 30000) {
  return new Promise((resolve, reject) => {
    const payload = {
      id,
      kind: options.kind || 'mermaid',
      source: options.source,
      out: options.out,
    };
    if (payload.kind === 'plantuml') {
      payload.mode = options.mode || 'remote';
      payload.dark = options.dark === true;
      if (payload.mode === 'native') {
        payload.exe = options.exe || '';
      } else {
        payload.server = options.server || 'https://kroki.io';
      }
    } else {
      payload.theme = options.theme || 'default';
    }

    const timer = setTimeout(() => {
      pending.delete(id);
      reject(new Error(`request ${id} timed out`));
    }, timeoutMs);
    pending.set(id, { resolve, reject, timer });
    const started = performance.now();
    child.stdin.write(JSON.stringify(payload) + '\n');
    pending.get(id).resolve = (message) => {
      message.wallMs = Math.round(performance.now() - started);
      resolve(message);
    };
  });
}

function svgFacts(file) {
  const svg = fs.readFileSync(file, 'utf8');
  return {
    bytes: Buffer.byteLength(svg, 'utf8'),
    hasSvg: /<svg[\s>]/i.test(svg.slice(0, 500)),
    foreignObject: (svg.match(/<foreignObject/g) || []).length,
    text: (svg.match(/<text/g) || []).length,
  };
}

function browserProcessInfo(pid) {
  if (!pid) {
    return null;
  }
  try {
    const script = [
      `$p = Get-Process -Id ${pid} -ErrorAction SilentlyContinue`,
      'if ($p) {',
      '  [pscustomobject]@{ Id=$p.Id; MainWindowHandle=$p.MainWindowHandle; MainWindowTitle=$p.MainWindowTitle } | ConvertTo-Json -Compress',
      '} else {',
      "  'null'",
      '}',
    ].join('\n');
    const output = execFileSync('powershell', ['-NoProfile', '-Command', script], {
      encoding: 'utf8',
    }).trim();
    return JSON.parse(output);
  } catch (error) {
    return { error: String(error) };
  }
}

async function main() {
  const startedAt = performance.now();
  await Promise.race([
    ready.promise,
    new Promise((_, reject) => setTimeout(() => reject(new Error('sidecar ready timeout')), 20000)),
  ]);
  console.log(`sidecar ready: ${ready.info.startupMs}ms`);
  console.log(`spawn->ready: ${Math.round(performance.now() - startedAt)}ms`);

  const mermaidCases = [
    { name: 'flow', source: 'graph TD; A[开始]-->B[结束];' },
    { name: 'complex', source: 'graph TD\n  A[开始] -->|条件| B{判断}\n  B -->|是| C[执行]\n  B -->|否| D[结束]\n  subgraph 子流程\n    C --> E[记录]\n  end' },
    { name: 'sequence', source: 'sequenceDiagram\n  Alice->>Bob: Hello Bob\n  Bob-->>Alice: Hi Alice' },
    { name: 'class', source: 'classDiagram\n  class Animal {\n    +int age\n    +isMammal() bool\n  }\n  class Duck {\n    +swim()\n  }\n  Animal <|-- Duck' },
    { name: 'gantt', source: 'gantt\n  title A Gantt\n  dateFormat YYYY-MM-DD\n  section S\n  Task :a1, 2026-01-01, 30d' },
    { name: 'dark', source: 'graph TD; A[开始]-->B[结束];', theme: 'dark' },
  ];

  const results = [];
  for (let i = 0; i < mermaidCases.length; i++) {
    const item = mermaidCases[i];
    const out = path.join(outDir, `${item.name}.svg`);
    const response = await request(i + 1, {
      kind: 'mermaid',
      source: item.source,
      out,
      theme: item.theme,
    });
    if (!response.ok) {
      throw new Error(`${item.name}: ${response.error}`);
    }
    const facts = svgFacts(out);
    let darkPalette;
    if (item.theme === 'dark') {
      const darkSvg = fs.readFileSync(out, 'utf8');
      darkPalette = {
        nodeFill: /#2f3542/i.test(darkSvg),
        line: /#c0caf5/i.test(darkSvg),
      };
      if (!darkPalette.nodeFill || !darkPalette.line) {
        throw new Error('dark Mermaid palette is missing node fill or line color');
      }
    }
    results.push({
      name: item.name,
      kind: 'mermaid',
      theme: item.theme || 'default',
      ms: response.ms,
      wallMs: response.wallMs,
      ...(darkPalette ? { darkPalette } : {}),
      ...facts,
    });
    console.log(
      `${item.name.padEnd(9)} kind=mermaid report=${String(response.ms).padStart(4)}ms wall=${String(response.wallMs).padStart(4)}ms ` +
      `bytes=${String(facts.bytes).padStart(6)} foreignObject=${facts.foreignObject} text=${facts.text}` +
      (darkPalette ? ` nodeFill=${darkPalette.nodeFill} line=${darkPalette.line}` : ''),
    );
  }

  await Promise.race([
    browserReady.promise,
    new Promise((_, reject) => setTimeout(() => reject(new Error('browser-ready timeout')), 20000)),
  ]);
  console.log(`browser: ${browserReady.info.edge} pid=${browserReady.info.browserPid}`);

  const plantUmlOut = path.join(outDir, 'plantuml.svg');
  const plantUmlResponse = await request(90, {
    kind: 'plantuml',
    source: '@startuml\nAlice -> Bob: hello\nBob --> Alice: hi\n@enduml\n',
    out: plantUmlOut,
    server: 'https://kroki.io',
  });
  let plantUmlRemote;
  if (!plantUmlResponse.ok) {
    // This leg depends on the public Kroki service; network failures are
    // reported but do not hide the local/Mermaid results.
    plantUmlRemote = { ok: false, error: plantUmlResponse.error || '' };
    console.warn(`plantuml remote: ${plantUmlRemote.error}`);
  } else {
    const plantUmlFacts = svgFacts(plantUmlOut);
    plantUmlRemote = { ok: true, ...plantUmlFacts };
    results.push({
      name: 'plantuml',
      kind: 'plantuml',
      ms: plantUmlResponse.ms,
      wallMs: plantUmlResponse.wallMs,
      ...plantUmlFacts,
    });
    console.log(
      `plantuml  kind=plantuml report=${String(plantUmlResponse.ms).padStart(4)}ms wall=${String(plantUmlResponse.wallMs).padStart(4)}ms ` +
      `bytes=${String(plantUmlFacts.bytes).padStart(6)} foreignObject=${plantUmlFacts.foreignObject} text=${plantUmlFacts.text}`,
    );
  }

  const nativeMissing = await request(91, {
    kind: 'plantuml',
    mode: 'native',
    source: '@startuml\nA -> B\n@enduml\n',
    out: path.join(outDir, 'plantuml-native-missing.svg'),
    exe: 'C:\\nonexistent\\plantuml-headless.exe',
  });
  console.log(
    `plantuml-native missing ok=${nativeMissing.ok} error=${(nativeMissing.error || '').slice(0, 80)}`,
  );
  if (nativeMissing.ok) {
    throw new Error('native PlantUML should fail without the executable');
  }

  const nativeExe = process.env.AWM_TEST_PLANTUML_NATIVE;
  let plantUmlNative;
  if (nativeExe) {
    const nativeOut = path.join(outDir, 'plantuml-native.svg');
    const nativeResponse = await request(93, {
      kind: 'plantuml',
      mode: 'native',
      source: '@startuml\nAlice -> Bob: 你好，世界\n@enduml\n',
      out: nativeOut,
      exe: nativeExe,
    });
    if (!nativeResponse.ok) {
      throw new Error(`plantuml-native: ${nativeResponse.error}`);
    }
    const nativeFacts = svgFacts(nativeOut);
    const nativeText = fs.readFileSync(nativeOut, 'utf8');
    const hasChinese = /你好，世界/.test(nativeText) || /&#20320;/.test(nativeText);
    const lightWhiteBackground = /background:#FFFFFF/i.test(nativeText);
    if (lightWhiteBackground) {
      throw new Error('native light render should have a transparent background');
    }

    const nativeDarkOut = path.join(outDir, 'plantuml-native-dark.svg');
    const nativeDarkResponse = await request(94, {
      kind: 'plantuml',
      mode: 'native',
      dark: true,
      source: '@startuml\nAlice -> Bob: hello\n@enduml\n',
      out: nativeDarkOut,
      exe: nativeExe,
    });
    if (!nativeDarkResponse.ok) {
      throw new Error(`plantuml-native-dark: ${nativeDarkResponse.error}`);
    }
    const nativeDarkFacts = svgFacts(nativeDarkOut);
    const nativeDarkText = fs.readFileSync(nativeDarkOut, 'utf8');
    const darkWhiteBackground = /background:#FFFFFF/i.test(nativeDarkText);
    const darkLightText = /fill="#FFFFFF"/i.test(nativeDarkText);
    const darkMessageWhite = /fill="#FFFFFF"[^>]*>hello/.test(nativeDarkText);
    if (darkWhiteBackground || !darkLightText || !darkMessageWhite) {
      throw new Error('native dark render should use a transparent dark theme');
    }

    plantUmlNative = {
      ok: true,
      hasChinese,
      dark: true,
      lightWhiteBackground,
      darkWhiteBackground,
      darkLightText,
      darkMessageWhite,
      ...nativeFacts,
    };
    results.push({
      name: 'plantuml-native',
      kind: 'plantuml',
      mode: 'native',
      ms: nativeResponse.ms,
      wallMs: nativeResponse.wallMs,
      hasChinese,
      ...nativeFacts,
    });
    results.push({
      name: 'plantuml-native-dark',
      kind: 'plantuml',
      mode: 'native',
      dark: true,
      ms: nativeDarkResponse.ms,
      wallMs: nativeDarkResponse.wallMs,
      ...nativeDarkFacts,
    });
    console.log(
      `plantuml-native kind=plantuml report=${String(nativeResponse.ms).padStart(4)}ms ` +
      `wall=${String(nativeResponse.wallMs).padStart(4)}ms bytes=${String(nativeFacts.bytes).padStart(6)} ` +
      `foreignObject=${nativeFacts.foreignObject} text=${nativeFacts.text} ` +
      `chinese=${hasChinese} whiteBg=${lightWhiteBackground}`,
    );
    console.log(
      `plantuml-native-dark kind=plantuml report=${String(nativeDarkResponse.ms).padStart(4)}ms ` +
      `wall=${String(nativeDarkResponse.wallMs).padStart(4)}ms bytes=${String(nativeDarkFacts.bytes).padStart(6)} ` +
      `whiteBg=${darkWhiteBackground} lightText=${darkLightText} messageWhite=${darkMessageWhite}`,
    );
  }

  const invalidResponse = await request(99, {
    kind: 'mermaid',
    source: 'graph TD; A[',
    out: path.join(outDir, 'invalid.svg'),
  });
  console.log(`invalid  ok=${invalidResponse.ok} error=${invalidResponse.error || ''}`);

  const processInfo = browserProcessInfo(browserReady.info.browserPid);
  console.log('edge process:', JSON.stringify(processInfo));

  console.log('waiting for idle shutdown...');
  const exitPromise = Promise.race([
    once(child, 'exit'),
    new Promise((_, reject) => setTimeout(() => reject(new Error('idle shutdown timeout')), 30000)),
  ]);
  const [code, signal] = await exitPromise;
  console.log(`idle exit: code=${code} signal=${signal}`);

  const report = {
    generatedAt: new Date().toISOString(),
    sidecarStartupMs: ready.info.startupMs,
    browser: browserReady.info,
    edgeProcess: processInfo,
    results,
    plantUmlRemote,
    plantUmlNative,
    invalid: { ok: invalidResponse.ok, error: invalidResponse.error || '' },
    nativeMissing: { ok: nativeMissing.ok, error: nativeMissing.error || '' },
    idleExit: { code, signal },
    stderr: stderrText.trim().split('\n').slice(-20),
  };
  fs.writeFileSync(path.join(outDir, 'report.json'), JSON.stringify(report, null, 2));
  console.log(`report: ${path.join(outDir, 'report.json')}`);
}

main().catch(async (error) => {
  console.error('FAILED:', error && error.message ? error.message : error);
  console.error(stderrText.slice(-2000));
  try {
    child.kill();
  } catch {
    // Ignore.
  }
  process.exitCode = 1;
});
