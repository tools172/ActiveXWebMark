# Diagram sidecar（Mermaid + PlantUML）

为 ActiveXWebMark 生成图表 SVG：

- **Mermaid**：官方 mermaid.js 在无界面的 Edge/Chrome 里运行（IE11 不能执行
  现代 mermaid.js）。
- **PlantUML 本地模式**：调用随安装包附带的官方 GraalVM 原生
  `plantuml-headless.exe`，**不需要 Java**，源码不离开本机。
- **PlantUML 远程模式**：POST 到 Kroki 兼容服务（默认 `https://kroki.io`）。
- **背景处理**：默认注入 `skinparam backgroundColor transparent`，让图融入预览；
  深色主题再注入 `!theme cyborg`，否则 PlantUML 默认的深色文字在暗背景上不可读。
  图源自己写了 `!theme` 或 `skinparam backgroundColor` 时完全尊重，不注入。
- 外挂本身只负责生成 SVG，缓存和预览显示由应用完成。

## 前提

- Node.js 18+
- Mermaid：本机安装 Edge（Windows 10/11 自带）或 Chrome；浏览器在第一次
  Mermaid 请求时才启动。
- PlantUML 本地模式：`plantuml-headless.exe` 及其 DLL 由安装包携带。
- PlantUML 远程模式：能访问配置的 Kroki 服务。

用户机器不需要 `npm install`：应用只使用构建好的 `sidecar.cjs` 和
`mermaid.min.js`。

## 构建并更新到 third_party

```powershell
cd tools/mermaid-sidecar
npm install
npm run vendor
```

`npm run vendor` 会打包 `sidecar.mjs`（含 `puppeteer-core`），并复制
`sidecar.cjs` 与 `mermaid.min.js` 到 `third_party/mermaid/`。

PlantUML 原生包由 `tools/plantuml-native/fetch.ps1` 单独下载、校验和暂存。

## 测试

```powershell
npm test
```

会验证：

1. 外挂启动（Edge 懒启动，应该很快）
2. 第一次 Mermaid 渲染（含 Edge 启动）与后续热渲染
3. 通过 Kroki 渲染 PlantUML（需要网络；失败只警告，不影响其它用例）
4. 本地 PlantUML 在原生 exe 无效时返回错误
5. 设置 `AWM_TEST_PLANTUML_NATIVE` 时测试真实原生渲染
6. 输出 SVG 是否包含 `foreignObject`（Mermaid 在 IE11 下必须为 0）
7. 深浅主题切换
8. 错误图源的返回
9. Edge 是否为无窗口运行（`MainWindowHandle` 应为 0）
10. 空闲 20 秒后是否自动退出

结果在 `out/report.json`，SVG 在 `out/*.svg`。

本地 PlantUML 的实际测试示例：

```powershell
$env:AWM_TEST_PLANTUML_NATIVE = 'C:\path\to\plantuml-headless.exe'
npm test
```

## 协议

stdin/stdout 每行一个 JSON：

```json
{"id":1,"kind":"mermaid","source":"graph TD; A-->B;","out":"C:/tmp/d1.svg","theme":"default"}
{"id":2,"kind":"plantuml","mode":"remote","source":"@startuml\nA -> B\n@enduml","out":"C:/tmp/d2.svg","server":"https://kroki.io","dark":false}
{"id":3,"kind":"plantuml","mode":"native","source":"@startuml\nA -> B\n@enduml","out":"C:/tmp/d3.svg","exe":"C:/app/plantuml/plantuml-headless.exe","dark":true}
```

```json
{"id":1,"ok":true,"kind":"mermaid","ms":42,"bytes":1234,"out":"C:/tmp/d1.svg"}
{"id":1,"ok":false,"kind":"mermaid","error":"..."}
```

- `kind`：`mermaid`（默认）或 `plantuml`。
- `theme`：Mermaid 的 `default` / `dark`。深色使用 `dark` 主题 + 一小组
  `themeVariables`（节点 `#2f3542`、边框 `#7aa2f7`、连线/箭头 `#c0caf5`、
  透明背景），避免黑色实心节点和看不见的连线。
- `mode`：PlantUML 的 `remote`（默认）或 `native`。
- `server`：远程模式的 Kroki 服务地址；缺省用 `AWM_PLANTUML_SERVER` 或
  `https://kroki.io`。
- `exe`：本地模式的 `plantuml-headless.exe` 路径。
- `dark`：预览是否深色；深色时注入 PlantUML `cyborg` 主题。
- `out` 为空时，SVG 会放在响应的 `svg` 字段里（大图建议始终给 `out`）。
- PlantUML 源码没有 `@start...` 时会自动包一层 `@startuml ... @enduml`。
- 本地模式设置 `PLANTUML_SECURITY_PROFILE=SANDBOX`，30 秒超时，输出上限 20MB。

## 环境变量

- `AWM_EDGE`：指定 Edge/Chrome 路径（Mermaid）。
- `AWM_PLANTUML_MODE`：`auto` / `local` / `remote`（由应用读取）。
- `AWM_PLANTUML_EXE`：本地模式的 `plantuml-headless.exe` 路径（由应用读取）。
- `AWM_PLANTUML_SERVER`：远程模式的 PlantUML 服务地址。
- `AWM_MERMAID_DISABLE=1` / `AWM_PLANTUML_DISABLE=1` / `AWM_DIAGRAM_DISABLE=1`
  由应用侧读取，用于整体/分项禁用。

## 说明

- `--idle-ms` 控制空闲退出，默认 180000（3 分钟）；应用传入 300000（5 分钟）。
- Mermaid 输出前会按 `viewBox` 给根 SVG 补全 `width`/`height`，否则 IE11 会按
  默认 150px 高度渲染，图会很小。
- PlantUML 的 SVG 保留服务端/原生程序生成的原始尺寸/样式（通常白底）。
- PlantUML 本地模式每次渲染启动一个原生进程（约 0.1–0.6 秒），应用缓存会避免
  重复渲染。
- 每次请求按顺序执行；应用按源码/主题/后端哈希缓存，只重画改动过的图。
