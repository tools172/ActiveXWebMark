# ActiveXWebMark

一个原生 Win32/WTL Markdown 阅读器（阅读优先，可切换编辑视图），使用 Scintilla
编辑、cmark-gfm 渲染、IE/MSHTML 预览。

## 特性

- **多标签**：一次打开多个 Markdown 文件，支持打开对话框多选与命令行多文件；
  标签可切换、关闭（关闭按钮/中键），未保存的标签显示脏标记；悬停显示完整路径，
  右键菜单支持重新加载、关闭当前/其他/左侧/右侧、在资源管理器中显示及复制路径/文件名。
- **拖拽打开**：从资源管理器把 Markdown 文件拖到窗口任意位置即可打开——标题栏、标签栏、
  编辑器、预览区和右侧目录都支持；拖动时显示系统的文件拖拽图标。一次拖入多个文件会各开
  一个标签，已打开的文件则切换到对应标签；非 Markdown 文件显示禁用光标，不会被打开。
- **会话恢复**：退出时保存所有打开的标签和当前标签，下次启动自动恢复；
  已不存在或无法读取的文件会被跳过。
- **最近打开**：文件菜单显示前 20 条记录，“更多…”分页加载全部记录；
  记录持久化到设置文件。
- **右侧目录导航**：按标题层级展示可折叠的目录树，点击定位到预览锚点与编辑器行号；
  导航面板可通过头部按钮或 Ctrl+B 折叠，展开时可拖动分割条调整宽度。
- **灵活分屏**：预览与编辑器支持左右分屏、上下分屏，分割条可拖动；两种方向分别
  记住比例。编辑器默认隐藏，Ctrl+E 随时切换。
- **滚动同步**：编辑视图与预览视图按 Markdown 源码位置双向同步；图片、代码块等
  高度不一致的块也能对齐。可在“视图 → 滚动同步”关闭。
- **查找**：Ctrl+F 打开/关闭查找栏，支持区分大小写、全词匹配与正则表达式；匹配结果在
  预览中高亮，F3 / Shift+F3 在匹配项之间跳转并自动回绕。
- **完整保存流程**：Ctrl+S 保存、Ctrl+Shift+S 另存为；编辑后标签显示脏标记，
  关闭标签或退出时提供“保存/不保存/取消”选择。写入采用临时文件原子替换。
- **代码高亮与主题**：预览中的围栏代码块由内置 PrismJS 1.29 高亮（cpp/c/c#/java/go/
  python/js/ts/jsx/tsx/json/yaml/ini/bash/powershell/sql/rust/markdown/diff/docker/
  html/css 等）；内置 53 套主题，一次切换同时作用于预览、编辑器和自绘主窗口界面，
  选择会持久化。主题菜单按“浅色/深色”“按作者/来源”“按色系”三种方式分类，
  完整目录见 [docs/themes.md](docs/themes.md)。
- **代码复制**：鼠标悬停在预览中的代码块时显示半透明复制图标，悬停图标显示“复制”提示，
  点击即可把该代码块的纯文本内容复制到剪贴板，无需手动选中。
- **自定义主题**：“视图 → 主题 → 主题设置...”可新建、复制、编辑、导入、删除和
  导出主题，并带真实 Markdown 预览。支持导入 VS Code Color Theme JSON/JSONC
  （含 `include` 与 `tokenColors`）、VSIX 主题包、TextMate `.tmTheme` 和
  Base16/Base24。用户主题保存在 `%LOCALAPPDATA%\ActiveXWebMark\themes\`，
  格式与 Schema 见 [docs/themes.md](docs/themes.md) 和
  [docs/awmtheme.schema.json](docs/awmtheme.schema.json)。
- **主题化滚动条**：预览文档/代码块以及编辑器、导航树、最近文件列表的竖直/水平
  滚动条都按主题的轨道、滑块、箭头颜色绘制；原生滚动条保留系统交互行为。
- **精致界面与主题**：自绘标题栏与窗口按钮，菜单与标题合并到标题栏一行（图标 →
  菜单 → 标题 → 最小化/最大化/关闭），标签栏位于其下；自绘现代标签栏、分割条与
  导航面板，细边框、悬停反馈、DPI 缩放；原生控件启用 Common Controls v6 与
  Explorer 主题，编辑器/预览配色随主题统一。
- **Mermaid / PlantUML 图表**：
  - `mermaid` 围栏代码块由随安装包附带的 Node 外挂调用官方 mermaid.js，在本机
    Edge/Chrome 的无界面进程中渲染成 SVG；
  - `plantuml` / `puml` 围栏代码块默认自动选择：找到随安装包附带的
    PlantUML headless 原生程序时本地渲染；否则发送到 Kroki 兼容的远程服务
    （默认 `https://kroki.io`）；
  - 本地模式可强制开启，源码不离开本机；远程服务器可配置或自建；
  - 结果按源码哈希缓存在本地；Mermaid 浅色用 `default` 主题，深色用 `dark` 主题 +
    精简 `themeVariables`（深蓝灰节点、亮色边框/连线、透明背景），不逐套匹配应用
    主题；PlantUML 根据明暗调整背景与配色；
  - Mermaid 需要 Node.js 18+ 和 Edge/Chrome；PlantUML 本地模式不需要 Java，
    远程模式需要网络。任一条件不满足时，原始代码块会原样显示，不影响其它功能。
- **安全预览**：相对图片与链接通过 `<base>` 正确解析；外部链接交给系统浏览器打开，
  预览不会被导航走；页面内 `#锚点` 链接在预览内滚动。
- **在线更新**：启动后后台检测一次 GitHub Releases 是否有新版本，发现后提示
  “立即升级 / 下次再说 / 忽略此版本”；“帮助 → 检查更新”可随时手动检测。下载过程在
  浮动小窗口中显示进度，完成后可一键退出程序并启动安装包。
- **关于**：帮助 → 关于（F1）显示软件名、版本号、构建配置、技术栈与第三方组件。

## 快捷键

| 快捷键 | 功能 |
| --- | --- |
| `Ctrl+O` | 打开 Markdown 文件（可多选） |
| `Ctrl+S` | 保存当前标签 |
| `Ctrl+Shift+S` | 另存为 |
| `Ctrl+W` | 关闭当前标签 |
| `Ctrl+Tab` / `Ctrl+Shift+Tab` | 下一个 / 上一个标签 |
| `Ctrl+E` | 显示/隐藏编辑器 |
| `Ctrl+F` | 打开/关闭查找栏 |
| `F3` / `Shift+F3` | 下一个 / 上一个匹配项 |
| `Esc` | 关闭查找栏 |
| `Ctrl+B` | 显示/隐藏右侧导航目录 |
| `Ctrl+Shift+L`（或 `Ctrl+Alt+L`） | 切换左右/上下分屏 |
| `Alt` / `F10` | 进入标题栏菜单导航；方向键选择，Enter/↓ 打开，Esc 退出 |
| `Alt` + 菜单字母 | 直接打开对应菜单（如 `Alt+F` 文件、`Alt+V` 视图） |
| `F1` | 关于 ActiveXWebMark（名称、版本、构建配置、技术栈） |

滚动同步可在“视图 → 滚动同步”中开关，默认开启。

## 技术栈

C++17 · WTL 10 · Scintilla · cmark-gfm（CommonMark + GFM 表格） · discount · IE/MSHTML 预览 ·
PrismJS 1.29（代码高亮，MIT） · Mermaid 11（图表渲染，MIT） ·
PlantUML（本地原生 exe 或远程 Kroki） · puppeteer-core（Mermaid 外挂，Apache-2.0） ·
miniz（VSIX ZIP 读取，MIT） · GDI+ 自绘 · WinHTTP（更新检查与下载） ·
CMake · vcpkg（manifest 模式） · GoogleTest

Mermaid 渲染需要用户环境提供 Node.js 18+ 和 Edge/Chrome；PlantUML 默认把源码
发送到配置的远程服务。缺少条件时自动回退到源码。

主题配色参考 [MarkText](https://github.com/marktext/marktext)（MIT）与
[VS Code](https://github.com/microsoft/vscode)（MIT）；作者归属与出处见
[docs/themes.md](docs/themes.md)。

## 浏览器兼容性

预览使用 IE/MSHTML。启动时程序会把当前 EXE 名称写入
`HKCU\Software\Microsoft\Internet Explorer\Main\FeatureControl\FEATURE_BROWSER_EMULATION`
（值 `11001`），让内嵌 WebBrowser 使用 IE11 文档模式。否则 Windows 会默认退回到
IE7/IE8 模式，`rgba()` 主题色与 PrismJS 代码高亮都无法工作。该键按用户、按 EXE 名称
生效，程序退出时不会删除；如果系统禁止写 HKCU，预览仍能显示，但代码块不会高亮。

### Mermaid / PlantUML 图表

IE11 不能执行现代 mermaid.js，所以图表不是由预览进程生成的。

**Mermaid**

- 应用按需启动 Node 外挂（`<exe>\mermaid\sidecar.cjs`）；
- 外挂在后台用无界面的 Edge/Chrome 运行官方 mermaid 11，生成 SVG；
- 首次渲染约 1–2 秒（需要启动 Edge），同一张图之后直接读缓存；
- 浅色预览用 Mermaid `default` 主题；深色预览用 `dark` 主题并覆盖一小组
  `themeVariables`：节点深蓝灰填充、亮色边框、亮色连线/箭头/虚线、透明背景，
  不会出现黑色实心节点或看不见的连线；不逐套匹配应用主题；
- 没有 Node.js 18+、版本过低、缺少 Edge/Chrome 或外挂文件缺失时，保留原始代码块。

**PlantUML**

- 默认 `auto`：找到随安装包附带的 `plantuml-headless.exe` 就本地渲染，否则
  POST 到 `https://kroki.io/plantuml/svg`；
- 本地模式：
  - 程序位于 `<exe>\plantuml\plantuml-headless.exe`（安装包自带）；
  - `diagrams.plantuml.mode=local`（或 `AWM_PLANTUML_MODE=local`）强制本地；
    也可用 `diagrams.plantuml.exe` / `AWM_PLANTUML_EXE` 指定其它位置；
  - 使用官方 GraalVM 原生构建，**不需要 Java**，源码不离开本机；
  - 使用 `PLANTUML_SECURITY_PROFILE=SANDBOX`、30 秒超时；
- 背景与配色：默认注入 `skinparam backgroundColor transparent`，浅色主题下图融入
  预览；深色主题再注入 `!theme cyborg`，避免默认深色文字在暗背景上不可读。图源
  自己写了 `!theme` 或 `skinparam backgroundColor` 时保持原样；
- 远程模式：
  - 服务器可用 `diagrams.plantuml.server` 或 `AWM_PLANTUML_SERVER` 配置；
    自建 Kroki 可离线/内网使用；
  - **图表源码会发送到该服务器**；不希望外发时用本地模式或
    `diagrams.plantuml.enabled=false` / `AWM_PLANTUML_DISABLE=1`；
  - 使用 POST，不受 IE 的 GET URL 长度限制影响；
- `mode=local` 但原生程序不可用时，保留原始代码块。

**缓存与开关**

- 两种图都写入 `%LOCALAPPDATA%\ActiveXWebMark\diagram-cache\`；
- 外挂空闲 5 分钟后退出；
- `AWM_DIAGRAM_DISABLE=1` 同时禁用两种；`AWM_MERMAID_DISABLE=1` 只禁用 Mermaid；
- `AWM_NODE` 指定 `node.exe`；`AWM_EDGE` 指定 Edge/Chrome（由外挂读取）。

主题作用于预览、编辑器与自绘主窗口控件。编辑器、导航树、最近文件列表的滚动条
（含轨道、滑块、箭头）由主题色重绘，预览滚动条由主题 CSS 着色；原生滚动条仍由
Windows 处理点击、拖动和滚轮。深色主题还会尝试启用 Windows 10/11 的暗色控件模式
（未公开的 uxtheme 接口），让原生菜单、TreeView/ListView 选择等跟随明暗；接口不可用
时这些系统部件保持系统样式。打开/保存对话框与 TreeView/ListView 表头仍由 Windows
绘制。

## 目录结构

```
third_party/   vcpkg 之外的头文件/源码库（WTL、PEG Markdown Highlight、PrismJS
               bundle、图表外挂与 mermaid bundle）
src/core/      纯 C++ 核心：文档模型 / 会话状态 / Markdown 引擎 / 高亮 / 设置 /
               最近文件 / 图表围栏块识别
src/core/diagram/  Mermaid/PlantUML 代码块识别、缓存键与占位 HTML（无 Windows
                   依赖，可单测）
src/core/update/  GitHub Release 解析与版本比较
src/layout/    纯几何库：分屏、导航、标签/标题栏布局与 TOC 层级（无 Windows 依赖，可单测）
src/ui/        可复用自绘控件：Theme / TitleBar / TabStrip / Splitter / NavPanel
src/editor/    编辑器抽象 + Scintilla 实现
src/preview/   预览抽象 + IE 实现（插件式，未来可加 WebView2 / Miniblink / litehtml）
src/app/       WTL 主程序：MainWindow / DocumentView / 菜单 / 对话框 / 持久化 /
               图表外挂进程管理
src/app/update/  更新检查、下载与安装程序启动
tools/mermaid-sidecar/  图表外挂源码（Mermaid + PlantUML），
                        npm run vendor 更新 third_party/mermaid
tools/plantuml-native/  PlantUML headless 原生包清单、下载/校验脚本与许可证
tests/         GoogleTest 单元测试（core + layout + ui）与手动冒烟脚本
```

模块依赖：`awm_core` ← `awm_layout` ← `awm_ui` ← `awm_app`；
`awm_editor` / `awm_preview` 依赖 `awm_core`。

## 构建要求

- Visual Studio 2022，安装「使用 C++ 的桌面开发」工作负载，**并勾选 ATL/MFC 组件**
  （WTL 依赖 ATL）。
- CMake ≥ 3.24。
- vcpkg（本机位于 `F:\vc_lib\vcpkg_2023`；如不同，请修改 `CMakePresets.json` 中的
  `CMAKE_TOOLCHAIN_FILE`，或设置环境变量 `VCPKG_ROOT` 后改用 `$env{VCPKG_ROOT}`）。
- **运行图表**需要 Node.js 18+；Mermaid 还需要 Edge/Chrome；PlantUML 本地模式
  使用随安装包附带的 headless 原生程序（打包时由 `tools/plantuml-native/fetch.ps1`
  下载校验），不需要用户安装 Java；也可使用远程 Kroki。构建本身不需要 Node：
  外挂与 mermaid bundle 已放在 `third_party/mermaid/`；需要更新时见
  `tools/mermaid-sidecar/README.md`。

## 构建

```powershell
cmake --preset vs2022
cmake --build --preset vs2022 --config Release
ctest --preset vs2022
```

产物位于 `build/vs2022/src/app/Release/ActiveXWebMark.exe`。

## 设置与数据

设置默认保存在 `%LOCALAPPDATA%\ActiveXWebMark\settings.json`，包括：

- 最近打开的文件列表；
- 编辑器/导航显隐、分屏方向与比例、导航宽度；
- 主题（`ui.theme`，如 `github-light`、`catppuccin-mocha`、`tokyo-night`、
  `vscode-dark-plus`、`github-dark` 等，完整列表见 `docs/themes.md`）；
- PlantUML：`diagrams.plantuml.enabled`（默认 `true`）、
  `diagrams.plantuml.mode`（`auto`/`local`/`remote`，默认 `auto`）、
  `diagrams.plantuml.server`（默认 `https://kroki.io`）、
  `diagrams.plantuml.exe`（可选，默认使用
  `<exe>\plantuml\plantuml-headless.exe`）；
- 窗口位置与最大化状态；
- 被“忽略此版本”的更新版本号（`update.ignoredVersion`）。

下载的更新安装包保存在 `%LOCALAPPDATA%\ActiveXWebMark\updates\`；Mermaid /
PlantUML SVG 缓存在 `%LOCALAPPDATA%\ActiveXWebMark\diagram-cache\`，可安全删除
（下次会重新渲染）。删除设置文件即可恢复默认设置，但不会删除已下载的安装包和图表缓存。

## CI/CD 与安装包

仓库包含两套 GitHub Actions 工作流：

- `.github/workflows/ci.yml`：push / PR 时在 Windows 上构建并跑单元测试。
- `.github/workflows/release.yml`：push `v*` tag 时构建，并同时生成两份安装程序：
  - WiX MSI —— `dist/ActiveXWebMark-<version>.msi`
  - NSIS Setup —— `dist/ActiveXWebMark-<version>-setup.exe`

安装包脚本位于 `packaging/`：`wix/ActiveXWebMark.wxs`（WiX v5）、
`nsis/ActiveXWebMark.nsi`（NSIS）。CI 通过 `vcpkg-configuration.json` 里声明的
overlay ports（`vcpkg-ports/`）复现本机的 discount `/J`、cmark-gfm/Scintilla 静态化补丁。

应用内更新读取 GitHub Releases API 的 latest release，优先下载 `*-setup.exe`，其次
`.msi`；因此每次发布都应附带这两类安装包（当前工作流会自动生成并上传）。

> 运行库依赖：程序以动态 CRT（`/MD`）链接，运行需要
> 「Microsoft Visual C++ 2015-2022 Redistributable (x64)」，绝大多数 Windows 10/11
> 已自带。NSIS 安装时如检测不到会给出提示。

## 发布

```powershell
# 三段式版本号，不带前缀（示例 1.2.3，实际以 CMakeLists.txt 的 project VERSION 为准）
$Version = "<major>.<minor>.<patch>"
git tag "v$Version"
git push origin "v$Version"
```

推送 tag 后 GitHub Actions 自动构建、跑测试并生成 MSI 与 Setup.exe，作为资产附在
对应的 GitHub Release 上。tag 统一使用 `v<major>.<minor>.<patch>` 三段格式，
`$Version` 换成当前版本号即可，本节命令不随版本变化。

发布工作流会把去掉 `v` 的 tag 版本通过 `-DAWM_VERSION` 注入程序，因此发布包的版本号
总与 tag 一致；本地开发构建则使用 `CMakeLists.txt` 中 `project(... VERSION ...)` 的
版本号。版本号只在 `CMakeLists.txt` 这一处维护，发版时改它并打上对应 tag 即可，
`vcpkg.json`、本文件和打包脚本都不再重复记录版本号。

## 开发协作

分支模型：`main`（稳定/发布）← `dev`（开发集成）← `feature/*` / `fix/*`。
贡献与发布流程见 [CONTRIBUTING.md](CONTRIBUTING.md)。代码风格见 `.clang-format`。

## 本地生成安装包

先构建程序，再用 WiX / NSIS 打包，产物输出到 `dist/`。版本号无需手动输入：
CMake 构建时会把 `CMakeLists.txt` 中 `project(... VERSION ...)` 的值写入
`build/vs2022/version.txt`，下面直接读取该文件。

```powershell
# 0. 下载并校验 PlantUML headless 原生包（约 40MB）
#    下载/校验失败会直接中止，不会生成安装包。
#    离线时可先手动下载，再执行：
#    tools\plantuml-native\fetch.ps1 -ZipPath D:\plantuml-headless.zip
powershell -ExecutionPolicy Bypass -File tools\plantuml-native\fetch.ps1

# 1. 构建（在 build/vs2022/src/app/Release/ 下生成 ActiveXWebMark.exe）
cmake --build --preset vs2022 --config Release

# 2. 版本号由 CMake 写入构建目录（取自 CMakeLists.txt 的 project VERSION）
$Version = (Get-Content build\vs2022\version.txt -Raw).Trim()
New-Item -ItemType Directory -Force dist | Out-Null

# 3. NSIS（安装 NSIS 后执行）
makensis "/DVERSION=$Version" packaging\nsis\ActiveXWebMark.nsi

# 4. WiX（安装 WiX 后，从仓库根目录执行）
dotnet tool install --global wix      # 仅首次
$env:WIX_VERSION = $Version
$env:AWM_EXE = (Resolve-Path "build\vs2022\src\app\Release\ActiveXWebMark.exe").Path
wix build -o "dist\ActiveXWebMark-$Version.msi" packaging\wix\ActiveXWebMark.wxs
```

PlantUML 原生包下载失败时会打印失败阶段、URL、预期/实际 SHA-256、日志路径，
并以非 0 退出；NSIS 脚本也会检查 `plantuml-headless.exe` 是否存在。
下载成功的 zip 缓存在 `build\plantuml-native\cache\`，后续打包不会重复下载。

产物（版本号取自上面的 `$Version`，以下 `<version>` 即该值）：
- `dist\ActiveXWebMark-<version>-setup.exe`（NSIS）
- `dist\ActiveXWebMark-<version>.msi`（WiX）

## 许可证

见各源文件头。第三方组件（discount：BSD-3；cmark-gfm：BSD-2；PMH：MIT；Scintilla：MIT；
WTL：MS-PL；mermaid：MIT；puppeteer-core：Apache-2.0）分别遵循其自身许可证。
Node.js 与 Edge/Chrome 由用户环境提供，不随本程序分发。PlantUML headless 原生
程序随安装包分发（GPLv3 或更高版本，许可证见
`tools/plantuml-native/LICENSE.plantuml` 与 `license.txt`）；
也可使用远程 Kroki，远程模式下图表源码会发送到配置的服务器。
