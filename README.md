# ActiveXWebMark

一个原生 Win32/WTL Markdown 阅读器（阅读优先，可切换编辑视图），使用 Scintilla
编辑、cmark 渲染、IE/MSHTML 预览。

## 特性

- **多标签**：一次打开多个 Markdown 文件，支持打开对话框多选与命令行多文件；
  标签可切换、关闭（关闭按钮/中键），未保存的标签显示脏标记；悬停显示完整路径，
  右键菜单支持关闭当前/其他/左侧/右侧、在资源管理器中显示及复制路径/文件名。
- **会话恢复**：退出时保存所有打开的标签和当前标签，下次启动自动恢复；
  已不存在或无法读取的文件会被跳过。
- **最近打开**：文件菜单显示前 20 条记录，“更多…”分页加载全部记录，可一键清除；
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
  html/css 等）；内置 6 套主题（GitHub Light、Ulysses、Solarized Light、One Dark、
  Dracula、Nord），一次切换同时作用于预览、编辑器和自绘主窗口界面，选择会持久化。
  主题菜单位于“视图 → 主题”。
- **精致界面与主题**：自绘标题栏与窗口按钮，菜单与标题合并到标题栏一行（图标 →
  菜单 → 标题 → 最小化/最大化/关闭），标签栏位于其下；自绘现代标签栏、分割条与
  导航面板，细边框、悬停反馈、DPI 缩放；原生控件启用 Common Controls v6 与
  Explorer 主题，编辑器/预览配色随主题统一。
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

C++17 · WTL 10 · Scintilla · cmark（CommonMark） · discount · IE/MSHTML 预览 ·
PrismJS 1.29（代码高亮，MIT） · GDI+ 自绘 · WinHTTP（更新检查与下载） · CMake ·
vcpkg（manifest 模式） · GoogleTest

主题配色参考 [MarkText](https://github.com/marktext/marktext)（MIT）。

## 浏览器兼容性

预览使用 IE/MSHTML。启动时程序会把当前 EXE 名称写入
`HKCU\Software\Microsoft\Internet Explorer\Main\FeatureControl\FEATURE_BROWSER_EMULATION`
（值 `11001`），让内嵌 WebBrowser 使用 IE11 文档模式。否则 Windows 会默认退回到
IE7/IE8 模式，`rgba()` 主题色与 PrismJS 代码高亮都无法工作。该键按用户、按 EXE 名称
生效，程序退出时不会删除；如果系统禁止写 HKCU，预览仍能显示，但代码块不会高亮。

主题作用于预览、编辑器与自绘主窗口控件；原生弹出菜单、打开/保存对话框、
TreeView/ListView 的表头与滚动条仍由 Windows 绘制。

## 目录结构

```
third_party/   vcpkg 之外的头文件/源码库（WTL、PEG Markdown Highlight、PrismJS bundle）
src/core/      纯 C++ 核心：文档模型 / 会话状态 / Markdown 引擎 / 高亮 / 设置 / 最近文件
src/core/update/  GitHub Release 解析与版本比较
src/layout/    纯几何库：分屏、导航、标签/标题栏布局与 TOC 层级（无 Windows 依赖，可单测）
src/ui/        可复用自绘控件：Theme / TitleBar / TabStrip / Splitter / NavPanel
src/editor/    编辑器抽象 + Scintilla 实现
src/preview/   预览抽象 + IE 实现（插件式，未来可加 WebView2 / Miniblink / litehtml）
src/app/       WTL 主程序：MainWindow / DocumentView / 菜单 / 对话框 / 持久化
src/app/update/  更新检查、下载与安装程序启动
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
- 主题（`ui.theme`，如 `github-light`、`one-dark`、`dracula` 等）；
- 窗口位置与最大化状态；
- 被“忽略此版本”的更新版本号（`update.ignoredVersion`）。

下载的更新安装包保存在 `%LOCALAPPDATA%\ActiveXWebMark\updates\`。删除设置文件即可
恢复默认设置，但不会删除已下载的安装包。

## CI/CD 与安装包

仓库包含两套 GitHub Actions 工作流：

- `.github/workflows/ci.yml`：push / PR 时在 Windows 上构建并跑单元测试。
- `.github/workflows/release.yml`：push `v*` tag 时构建，并同时生成两份安装程序：
  - WiX MSI —— `dist/ActiveXWebMark-<version>.msi`
  - NSIS Setup —— `dist/ActiveXWebMark-<version>-setup.exe`

安装包脚本位于 `packaging/`：`wix/ActiveXWebMark.wxs`（WiX v5）、
`nsis/ActiveXWebMark.nsi`（NSIS）。CI 通过 `vcpkg-configuration.json` 里声明的
overlay ports（`vcpkg-ports/`）复现本机的 discount `/J`、cmark/Scintilla 静态化补丁。

应用内更新读取 GitHub Releases API 的 latest release，优先下载 `*-setup.exe`，其次
`.msi`；因此每次发布都应附带这两类安装包（当前工作流会自动生成并上传）。

> 运行库依赖：程序以动态 CRT（`/MD`）链接，运行需要
> 「Microsoft Visual C++ 2015-2022 Redistributable (x64)」，绝大多数 Windows 10/11
> 已自带。NSIS 安装时如检测不到会给出提示。

## 发布

```powershell
git tag v0.2.0
git push origin v0.2.0
```

推送 tag 后 GitHub Actions 自动构建、跑测试并生成 MSI 与 Setup.exe，作为资产附在
对应的 GitHub Release 上。tag 建议使用 `v<major>.<minor>.<patch>` 三段格式。

发布工作流会把去掉 `v` 的 tag 版本通过 `-DAWM_VERSION` 注入程序，因此发布包的版本号
总与 tag 一致；本地开发构建则使用 `CMakeLists.txt` 中 `project(... VERSION ...)` 的
版本号，发布前请一并更新。

## 开发协作

分支模型：`main`（稳定/发布）← `dev`（开发集成）← `feature/*` / `fix/*`。
贡献与发布流程见 [CONTRIBUTING.md](CONTRIBUTING.md)。代码风格见 `.clang-format`。

## 本地生成安装包

先构建程序，再用 WiX / NSIS 打包，产物输出到 `dist/`。

```powershell
# 0. 构建（在 build/vs2022/src/app/Release/ 下生成 ActiveXWebMark.exe）
cmake --build --preset vs2022 --config Release

# 1. 确保输出目录存在
New-Item -ItemType Directory -Force dist | Out-Null

# 2. NSIS（安装 NSIS 后执行）
makensis /DVERSION=0.2.0 packaging\nsis\ActiveXWebMark.nsi

# 2. WiX（安装 WiX 后，从仓库根目录执行）
dotnet tool install --global wix      # 仅首次
$env:WIX_VERSION = "0.2.0"
$env:AWM_EXE = (Resolve-Path "build\vs2022\src\app\Release\ActiveXWebMark.exe").Path
wix build -o "dist\ActiveXWebMark-0.2.0.msi" packaging\wix\ActiveXWebMark.wxs
```

产物：
- `dist\ActiveXWebMark-0.2.0-setup.exe`（NSIS）
- `dist\ActiveXWebMark-0.2.0.msi`（WiX）

## 许可证

见各源文件头。第三方组件（discount：BSD-3；cmark：BSD-2；PMH：MIT；Scintilla：MIT；
WTL：MS-PL）分别遵循其自身许可证。
