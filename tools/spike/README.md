# Mermaid / PlantUML 外挂最小验证

验证目标：在**保留 IE11 预览**的前提下，用 Node + 官方 mermaid.js + 本机 Edge
生成 Mermaid SVG；PlantUML 走远程 Kroki 服务生成 SVG；两者都交给 IE11 显示。

结论：**方案可行**。IE11 只负责显示；Mermaid 在后台 Edge 里执行，PlantUML
由远程服务渲染。

## 实测结果

### 外挂本身（`tools/mermaid-sidecar`）

| 项目 | 结果 |
|---|---|
| 冷启动（Node + Edge + mermaid 初始化） | 约 1.0–1.7 秒 |
| 热渲染 flow | 52–55ms |
| 热渲染 complex（含 subgraph） | 27–30ms |
| 热渲染 sequence | 18–20ms |
| 热渲染 class | 33–34ms |
| 热渲染 gantt | 16–18ms |
| PlantUML（远程 Kroki） | 约 0.8–1.2 秒（网络/连接开销） |
| PlantUML（本地原生 exe） | 约 0.1–0.6 秒（无 Java） |
| `<foreignObject>` | 全部为 0 |
| Edge 窗口 | `MainWindowHandle = 0`，无可见窗口 |
| 空闲 5 秒退出 | 退出码 0 |
| 错误图源 | 返回 `ok:false` 和 Mermaid 解析错误 |
| 打包体积 | `sidecar.cjs` 2.27MB + `mermaid.min.js` 2.69MB |

### C++ 调用（`host`）

`awm_spike_host.exe` 通过 `CreateProcessW` 启动 Node 外挂，用 JSON 行通信：

- 第一张图：1188ms（含冷启动；外挂内部渲染 61ms）
- 第二张图：30ms（外挂内部 28ms）
- 两张 SVG 均写入文件，失败数 0

### IE11 显示

用现有 Release 版 `ActiveXWebMark.exe` 打开生成的 Markdown：

- **内联 SVG**：正常显示，中文标签、箭头、子图都正确。
- **`<img src="file:///...svg">`**：正常显示，按 `width` 缩放。
- **PlantUML SVG**：远程 Kroki 返回的 SVG 在 IE11 中正常显示。
- 阴影/动画等 CSS filter 被 IE11 忽略，纯视觉差异，不影响阅读。

## 关键兼容点

Mermaid 输出的根 `<svg>` 是 `width="100%"` 且**没有 `height`**。
IE11 会给它默认 150px 高度，图被缩得很小。外挂在输出前做了归一化：

```text
viewBox="0 0 309 503"
  -> width="309" height="503"
  -> style="max-width:100%;"
```

归一化后，内联 SVG 和 `<img>` 在 IE11 里都能按正常尺寸显示。

## 复现

```powershell
# 1. 构建并测试外挂
cd tools/mermaid-sidecar
npm install
npm run build
npm test

# 2. 构建 C++ 调用示例
cd ..\..
cmake -S tools/spike/host -B build/spike-host -G "Visual Studio 17 2022" -A x64
cmake --build build/spike-host --config Release

# 3. C++ 调用外挂，生成 SVG 和预览 Markdown
build\spike-host\Release\awm_spike_host.exe `
  tools\mermaid-sidecar\dist\sidecar.cjs `
  tools\mermaid-sidecar\dist\mermaid.min.js `
  tools\spike\host\out

# 4. 用现有应用打开预览
build\vs2022\src\app\Release\ActiveXWebMark.exe tools\spike\host\out\spike-preview.md

# 5. 正式集成验证（需要 Node 18+ 和 Edge/Chrome）
build\integration\src\app\Release\ActiveXWebMark.exe tools\spike\ie-preview\integration.md
```

`ie-preview` 目录里有拆分好的内联 SVG / `<img>` 用例和截图脚本。
`integration.md` 用于验证正式集成；`integration-error.md` 用于验证语法错误回退。

## 目录

```text
tools/mermaid-sidecar/  Node 外挂正式源码（sidecar.mjs、test.mjs、build.mjs）
tools/spike/
  host/                 C++ 调用示例（CreateProcessW + Job Object + JSON 行）
  ie-preview/           生成 IE11 预览用例和截图脚本
```

## 说明

- 这份目录保留了方案验证用的 C++ 调用示例和截图脚本；正式集成代码在
  `src/core/diagram/` 和 `src/app/diagramservice.cpp`。
- `node_modules/`、`dist/`、`out/` 都已忽略；构建产物不入库。
- 正式集成已完成：Mermaid/PlantUML fenced block 识别、异步调度、源码哈希缓存、
  Mermaid 主题切换、Node/Edge/远程服务缺失时回退源码、安装包携带外挂。
- 本地 PlantUML 验证：运行 `tools/plantuml-native/fetch.ps1` 下载并暂存官方
  headless 原生包；应用会从 `<exe>\plantuml\plantuml-headless.exe` 自动选择本地
  模式；也可设置 `AWM_PLANTUML_MODE=local`、`AWM_PLANTUML_EXE` 强制指定。
