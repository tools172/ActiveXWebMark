// Diagram -> SVG sidecar for ActiveXWebMark.
//
// Mermaid runs the official mermaid bundle in headless Edge/Chrome (IE11 cannot
// execute modern mermaid.js). PlantUML is rendered by a remote Kroki-compatible
// server, because a pure-Node PlantUML renderer does not exist.
//
// Protocol (one JSON object per line on stdin/stdout):
//   in : {"id":1,"kind":"mermaid","source":"graph TD; A-->B;","out":"C:/tmp/d1.svg","theme":"default"}
//        {"id":2,"kind":"plantuml","source":"@startuml ...","out":"C:/tmp/d2.svg","server":"https://kroki.io"}
//   out: {"id":1,"ok":true,"ms":42,"bytes":1234,"out":"C:/tmp/d1.svg"}
//        {"id":1,"ok":false,"error":"..."}
//
// Usage:
//   node sidecar.cjs [--idle-ms 180000] [--edge <path>] [--mermaid <path>]

import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import readline from 'node:readline';
import { execFileSync, spawn } from 'node:child_process';
import puppeteer from 'puppeteer-core';

// Works both as ESM (sidecar.mjs) and as a CJS esbuild bundle (sidecar.cjs).
const here = process.argv[1]
  ? path.dirname(path.resolve(process.argv[1]))
  : process.cwd();

function argValue(name, fallback) {
  const index = process.argv.indexOf(name);
  if (index < 0 || index + 1 >= process.argv.length) {
    return fallback;
  }
  return process.argv[index + 1];
}

const idleMs = Number(argValue('--idle-ms', '180000')) || 0;
const edgeOverride = argValue('--edge', '');
const mermaidPath = argValue('--mermaid', path.join(here, 'mermaid.min.js'));
const maxSourceBytes = 512 * 1024;
const defaultPlantUmlServer =
  process.env.AWM_PLANTUML_SERVER || 'https://kroki.io';

function log(event, extra = {}) {
  process.stderr.write(JSON.stringify({ event, ...extra }) + '\n');
}

function firstExisting(paths) {
  for (const candidate of paths) {
    if (candidate && fs.existsSync(candidate)) {
      return candidate;
    }
  }
  return '';
}

function edgeFromRegistry() {
  try {
    const key = 'HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\msedge.exe';
    const output = execFileSync('reg', ['query', key, '/ve'], { encoding: 'utf8' });
    const match = /REG_SZ\s+(.+?)\s*$/m.exec(output);
    if (match && fs.existsSync(match[1].trim())) {
      return match[1].trim();
    }
  } catch {
    // Registry fallback only; standard paths are checked first.
  }
  return '';
}

function findBrowser() {
  if (edgeOverride) {
    return fs.existsSync(edgeOverride) ? edgeOverride : '';
  }
  if (process.env.AWM_EDGE) {
    return fs.existsSync(process.env.AWM_EDGE) ? process.env.AWM_EDGE : '';
  }

  const programFiles = process.env['ProgramFiles'] || 'C:\\Program Files';
  const programFilesX86 = process.env['ProgramFiles(x86)'] || 'C:\\Program Files (x86)';
  const localAppData = process.env['LOCALAPPDATA'] || '';

  const candidates = [
    path.join(programFilesX86, 'Microsoft\\Edge\\Application\\msedge.exe'),
    path.join(programFiles, 'Microsoft\\Edge\\Application\\msedge.exe'),
    localAppData && path.join(localAppData, 'Microsoft\\Edge\\Application\\msedge.exe'),
    path.join(programFiles, 'Google\\Chrome\\Application\\chrome.exe'),
    path.join(programFilesX86, 'Google\\Chrome\\Application\\chrome.exe'),
    localAppData && path.join(localAppData, 'Google\\Chrome\\Application\\chrome.exe'),
  ];
  return firstExisting(candidates) || edgeFromRegistry();
}

let browser = null;
let page = null;
let browserError = '';
let userDataDir = '';
let idleTimer = null;
let shuttingDown = false;
let nextDiagramId = 0;
let currentTheme = 'default';
const startedAt = performance.now();

async function shutdown(reason) {
  if (shuttingDown) {
    return;
  }
  shuttingDown = true;
  if (idleTimer) {
    clearTimeout(idleTimer);
    idleTimer = null;
  }
  log('shutdown', { reason });
  try {
    if (browser) {
      await browser.close();
    }
  } catch {
    // Browser may already be gone; the parent kills the process tree anyway.
  }
  try {
    if (userDataDir) {
      fs.rmSync(userDataDir, { recursive: true, force: true });
    }
  } catch {
    // A locked temp profile is harmless; the next run uses a fresh one.
  }
  // Fatal means the sidecar itself cannot run; normal idle/eof is code 0.
  process.exit(reason === 'fatal' ? 1 : 0);
}

function suspendIdle() {
  if (idleTimer) {
    clearTimeout(idleTimer);
    idleTimer = null;
  }
}

function resetIdle() {
  if (!idleMs) {
    return;
  }
  suspendIdle();
  idleTimer = setTimeout(() => {
    void shutdown('idle');
  }, idleMs);
}

// The browser is started lazily so a document containing only PlantUML never
// pays the Edge startup cost.
// Mermaid's built-in dark theme fills nodes with near-black (#1f2020), which
// looks like a solid blob on a dark page. Keep the theme's light text and
// lines, but override the fill/border colors and make the background
// transparent. One curated palette serves every dark application theme.
const darkThemeVariables = {
  background: 'transparent',
  primaryColor: '#2f3542',
  primaryTextColor: '#e6e6e6',
  primaryBorderColor: '#7aa2f7',
  lineColor: '#c0caf5',
  secondaryColor: '#3b4261',
  tertiaryColor: '#24283b',
  mainBkg: '#2f3542',
  nodeBorder: '#7aa2f7',
  clusterBkg: '#292e42',
  clusterBorder: '#565f89',
  edgeLabelBackground: '#1f2335',
  textColor: '#e6e6e6',
  titleColor: '#e6e6e6',
  actorBkg: '#2f3542',
  actorBorder: '#7aa2f7',
  actorTextColor: '#e6e6e6',
  signalColor: '#c0caf5',
  signalTextColor: '#e6e6e6',
  labelBoxBkgColor: '#2f3542',
  labelBoxBorderColor: '#7aa2f7',
  labelTextColor: '#e6e6e6',
  loopTextColor: '#e6e6e6',
  noteBkgColor: '#3b4261',
  noteTextColor: '#e6e6e6',
  noteBorderColor: '#565f89',
  activationBkgColor: '#3b4261',
  activationBorderColor: '#7aa2f7',
};

function mermaidConfig(theme) {
  const config = {
    startOnLoad: false,
    securityLevel: 'strict',
    htmlLabels: false,
    flowchart: { htmlLabels: false },
    maxTextSize: 500000,
    fontFamily: '"Segoe UI","Microsoft YaHei UI","Microsoft YaHei",sans-serif',
    theme: theme === 'dark' ? 'dark' : 'default',
  };
  if (theme === 'dark') {
    config.themeVariables = darkThemeVariables;
  }
  return config;
}

async function ensureBrowser() {
  if (browser) {
    return browser;
  }
  if (browserError) {
    throw new Error(browserError);
  }
  if (!fs.existsSync(mermaidPath)) {
    browserError = `mermaid bundle not found: ${mermaidPath}`;
    throw new Error(browserError);
  }
  const browserPath = findBrowser();
  if (!browserPath) {
    browserError = 'Edge/Chrome not found; set AWM_EDGE or pass --edge <path>';
    throw new Error(browserError);
  }

  try {
    userDataDir = fs.mkdtempSync(path.join(os.tmpdir(), 'awm-diagram-'));
    browser = await puppeteer.launch({
      executablePath: browserPath,
      headless: true,
      userDataDir,
      args: [
        '--disable-gpu',
        '--disable-background-networking',
        '--disable-component-update',
        '--disable-sync',
        '--disable-extensions',
        '--disable-default-apps',
        '--no-first-run',
        '--no-default-browser-check',
        '--mute-audio',
        '--metrics-recording-only',
      ],
    });

    log('browser-ready', {
      edge: browserPath,
      browserPid: browser.process() ? browser.process().pid : 0,
      startupMs: Math.round(performance.now() - startedAt),
    });
    return browser;
  } catch (error) {
    browserError = String((error && error.message) || error);
    try {
      if (browser) {
        await browser.close();
      }
    } catch {
      // Ignore.
    }
    browser = null;
    page = null;
    throw new Error(browserError);
  }
}

async function ensureMermaidPage(theme) {
  const normalized = theme === 'dark' ? 'dark' : 'default';
  if (page && currentTheme === normalized) {
    return page;
  }

  // A fresh page per appearance avoids Mermaid's initialize() merging the
  // previous themeVariables into the new theme.
  if (page) {
    try {
      await page.close();
    } catch {
      // Ignore.
    }
    page = null;
  }

  page = await browser.newPage();
  await page.setRequestInterception(true);
  page.on('request', (request) => {
    const url = request.url();
    if (url === 'about:blank' || url.startsWith('file:') || url.startsWith('data:')) {
      request.continue();
    } else {
      request.abort();
    }
  });

  await page.setContent(
    '<!DOCTYPE html><html><head><meta charset="utf-8"></head><body></body></html>',
    { waitUntil: 'domcontentloaded' },
  );
  await page.addScriptTag({ path: mermaidPath });
  await page.evaluate((config) => {
    if (!window.mermaid) {
      throw new Error('mermaid bundle did not define window.mermaid');
    }
    window.mermaid.initialize(config);
  }, mermaidConfig(normalized));
  currentTheme = normalized;
  return page;
}

// IE11 gives an inline SVG with width="100%" but no height attribute a default
// height of 150px, which makes diagrams tiny. Mermaid only emits a viewBox.
// Give the root explicit pixel dimensions and let CSS scale it down if needed.
function normalizeSvg(svg) {
  const openTagMatch = /<svg\b[^>]*>/.exec(svg);
  if (!openTagMatch) {
    return svg;
  }
  let openTag = openTagMatch[0];

  const viewBox = /viewBox="([^"]+)"/.exec(openTag);
  if (!viewBox) {
    return svg;
  }
  const parts = viewBox[1].trim().split(/[,\s]+/).map(Number);
  if (parts.length !== 4 || !Number.isFinite(parts[2]) || !Number.isFinite(parts[3])) {
    return svg;
  }
  const width = Math.max(1, Math.round(parts[2]));
  const height = Math.max(1, Math.round(parts[3]));

  openTag = openTag.replace(/\swidth="[^"]*"/, '');
  openTag = openTag.replace(/\sheight="[^"]*"/, '');
  openTag = openTag.replace(/^<svg\b/, `<svg width="${width}" height="${height}"`);

  openTag = openTag.replace(/\sstyle="([^"]*)"/, (_, style) => {
    const cleaned = style.replace(/max-width\s*:[^;]*;?/g, '');
    return ` style="${cleaned}max-width:100%;"`;
  });
  if (!/\sstyle="/.test(openTag)) {
    openTag = openTag.replace(/^<svg\b/, '<svg style="max-width:100%;"');
  }

  return openTag + svg.slice(openTagMatch.index + openTagMatch[0].length);
}

async function renderMermaid(source, theme) {
  await ensureBrowser();
  const normalizedTheme = theme === 'dark' ? 'dark' : 'default';
  const mermaidPage = await ensureMermaidPage(normalizedTheme);

  const id = `awm-diagram-${nextDiagramId++}`;
  const svg = await mermaidPage.evaluate(
    async ({ elementId, text }) => {
      const { svg } = await window.mermaid.render(elementId, text);
      return svg;
    },
    { elementId: id, text: source },
  );
  if (typeof svg !== 'string' || !svg.startsWith('<svg')) {
    throw new Error('mermaid returned an unexpected result');
  }
  return normalizeSvg(svg);
}

function normalizePlantUmlSource(source) {
  if (/@start\w+/i.test(source)) {
    return source;
  }
  return `@startuml\n${source}\n@enduml\n`;
}

// Make diagrams blend into the preview. PlantUML defaults to a white
// background; on dark themes that would also leave its default dark text
// unreadable, so use PlantUML's built-in cyborg theme there.
// Explicit diagram styling is respected.
function applyPlantUmlAppearance(source, dark) {
  const text = normalizePlantUmlSource(source);
  if (/\b!theme\s+/i.test(text) || /^\s*skinparam\s+backgroundColor\b/im.test(text)) {
    return text;
  }

  const injections = [];
  if (dark) {
    injections.push('!theme cyborg');
    // Cyborg leaves some labels dark (sequence messages, class text).
    injections.push('skinparam sequenceMessageTextColor #FFFFFF');
    injections.push('skinparam classFontColor #FFFFFF');
    injections.push('skinparam classAttributeFontColor #FFFFFF');
    injections.push('skinparam classStereotypeFontColor #DDDDDD');
  }
  injections.push('skinparam backgroundColor transparent');

  const lines = text.split(/\r?\n/);
  const start = lines.findIndex((line) => /^\s*@start\w+/i.test(line));
  if (start < 0) {
    return text;
  }
  lines.splice(start + 1, 0, ...injections);
  return lines.join('\n');
}

async function renderPlantUml(source, server) {
  let base;
  try {
    base = new URL(server.endsWith('/') ? server : `${server}/`);
  } catch {
    throw new Error(`invalid PlantUML server: ${server}`);
  }
  if (base.protocol !== 'http:' && base.protocol !== 'https:') {
    throw new Error('PlantUML server must use http or https');
  }

  const endpoint = new URL('plantuml/svg', base);
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), 20000);
  try {
    const response = await fetch(endpoint, {
      method: 'POST',
      headers: { 'content-type': 'text/plain; charset=utf-8' },
      body: source,
      signal: controller.signal,
    });
    const text = await response.text();
    if (!response.ok) {
      throw new Error(
        `PlantUML server returned ${response.status}: ${text.slice(0, 200)}`,
      );
    }
    if (!/<svg[\s>]/i.test(text.slice(0, 500))) {
      throw new Error('PlantUML server returned no SVG');
    }
    return text;
  } catch (error) {
    if (error && error.name === 'AbortError') {
      throw new Error('PlantUML request timed out');
    }
    throw error;
  } finally {
    clearTimeout(timer);
  }
}

function runPlantUmlProcess(source, executable, args, extraEnv = {}) {
  return new Promise((resolve, reject) => {
    if (!executable || !fs.existsSync(executable)) {
      reject(new Error(`PlantUML executable not found: ${executable || '(empty)'}`));
      return;
    }

    let child;
    try {
      child = spawn(executable, args, {
        windowsHide: true,
        stdio: ['pipe', 'pipe', 'pipe'],
        env: { ...process.env, ...extraEnv },
      });
    } catch (error) {
      reject(error);
      return;
    }

    let stdout = Buffer.alloc(0);
    let stderr = '';
    let finished = false;
    const finish = (error, svg) => {
      if (finished) {
        return;
      }
      finished = true;
      clearTimeout(timer);
      if (error) {
        reject(error);
      } else {
        resolve(svg);
      }
    };

    const timer = setTimeout(() => {
      try {
        child.kill();
      } catch {
        // Ignore.
      }
      finish(new Error('PlantUML timed out'));
    }, 30000);

    child.stdout.on('data', (chunk) => {
      stdout = Buffer.concat([stdout, chunk]);
      if (stdout.length > 20 * 1024 * 1024) {
        try {
          child.kill();
        } catch {
          // Ignore.
        }
        finish(new Error('PlantUML output is too large'));
      }
    });
    child.stderr.on('data', (chunk) => {
      stderr = (stderr + chunk.toString('utf8')).slice(-4000);
    });
    child.on('error', (error) => finish(error));
    child.on('close', (code) => {
      const text = stdout.toString('utf8');
      if (code !== 0) {
        finish(new Error(stderr.trim() || `PlantUML exited with ${code}`));
        return;
      }
      if (!/<svg[\s>]/i.test(text.slice(0, 1000))) {
        finish(new Error(stderr.trim() || 'PlantUML produced no SVG'));
        return;
      }
      finish(null, text);
    });

    // The process may exit before consuming all input on a syntax error.
    child.stdin.on('error', () => {});
    child.stdin.end(source, 'utf8');
  });
}

function renderPlantUmlNative(source, exePath) {
  if (!exePath || !fs.existsSync(exePath)) {
    return Promise.reject(new Error('PlantUML native executable not found'));
  }
  return runPlantUmlProcess(
    source,
    exePath,
    ['-charset', 'UTF-8', '-pipe', '-tsvg'],
    { PLANTUML_SECURITY_PROFILE: 'SANDBOX' },
  );
}

async function writeSvg(out, svg) {
  fs.mkdirSync(path.dirname(out), { recursive: true });
  // Write-then-rename so the app never reads a partially written SVG.
  const temp = `${out}.${process.pid}.tmp`;
  fs.writeFileSync(temp, svg, 'utf8');
  try {
    fs.renameSync(temp, out);
  } catch {
    fs.copyFileSync(temp, out);
    fs.rmSync(temp, { force: true });
  }
}

async function handleLine(line) {
  let request;
  try {
    request = JSON.parse(line);
  } catch {
    return; // Ignore non-JSON lines such as blank keepalives.
  }

  const id = request.id;
  const source = typeof request.source === 'string' ? request.source : '';
  const out = typeof request.out === 'string' ? request.out : '';
  const kind = request.kind === 'plantuml' ? 'plantuml' : 'mermaid';
  const theme = request.theme === 'dark' ? 'dark' : 'default';
  const mode = request.mode === 'native' ? 'native' : 'remote';
  const server =
    typeof request.server === 'string' && request.server
      ? request.server
      : defaultPlantUmlServer;

  if (!source.trim()) {
    reply({ id, ok: false, error: 'empty diagram source' });
    return;
  }
  if (Buffer.byteLength(source, 'utf8') > maxSourceBytes) {
    reply({ id, ok: false, error: 'diagram source is too large' });
    return;
  }

  const started = performance.now();
  try {
    const plantumlSource =
      kind === 'plantuml'
        ? applyPlantUmlAppearance(source, request.dark === true)
        : source;
    const svg =
      kind === 'plantuml'
        ? mode === 'native'
          ? await renderPlantUmlNative(
              plantumlSource,
              typeof request.exe === 'string' ? request.exe : '',
            )
          : await renderPlantUml(plantumlSource, server)
        : await renderMermaid(source, theme);
    if (out) {
      await writeSvg(out, svg);
    }
    reply({
      id,
      kind,
      ok: true,
      ms: Math.round(performance.now() - started),
      bytes: Buffer.byteLength(svg, 'utf8'),
      out: out || '',
      svg: out ? undefined : svg,
    });
  } catch (error) {
    reply({
      id,
      kind,
      ok: false,
      ms: Math.round(performance.now() - started),
      error: String((error && error.message) || error),
    });
  } finally {
    resetIdle();
  }
}

function reply(message) {
  process.stdout.write(JSON.stringify(message) + '\n');
}

async function main() {
  // No browser is started here; Mermaid requests start it on demand.
  log('ready', { startupMs: Math.round(performance.now() - startedAt) });
  resetIdle();

  const lines = readline.createInterface({ input: process.stdin, crlfDelay: Infinity });
  for await (const line of lines) {
    if (shuttingDown) {
      break;
    }
    if (line.trim()) {
      // A remote/local render can take longer than the idle timeout; do not
      // let the timer exit the process while a request is in flight.
      suspendIdle();
      try {
        await handleLine(line);
      } finally {
        resetIdle();
      }
    }
  }
  await shutdown('eof');
}

process.on('SIGINT', () => void shutdown('sigint'));
process.on('SIGTERM', () => void shutdown('sigterm'));
process.on('SIGHUP', () => void shutdown('sighup'));

main().catch((error) => {
  log('fatal', { error: String((error && error.stack) || error) });
  void shutdown('fatal');
});
