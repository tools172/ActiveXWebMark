# ActiveXWebMark

一个脱离 Qt 的原生 Win32/WTL Markdown 阅读器（阅读优先，可切换编辑视图），基于
CuteMarkEd 的 Markdown 能力重写而来。

## 特性

- **目录导航**：左侧目录树按标题层级展示，点击定位到对应章节（预览锚点 + 编辑器行号）。
- **阅读优先**：默认只显示预览，编辑视图可按需切换显示。
- **可替换预览引擎**：预览通过 `IPreviewView` 接口抽象，首版实现为 IE WebBrowser（MSHTML，
  单进程、系统自带），未来可无缝切换到 Miniblink / WebView2 / litehtml。
- **模块化**：`core`（纯逻辑）/ `editor` / `preview` / `app` 四层，接口高度抽象、依赖注入。

## 技术栈

C++17 · WTL 10 · Scintilla · discount · IE(MSHTML) · CMake · vcpkg(manifest) · GoogleTest

## 目录结构

```
third_party/   vcpkg 之外的头文件库（WTL、PEG Markdown Highlight）
src/core/      纯 C++ 核心：文档模型 / Markdown 引擎 / 高亮 / 设置
src/editor/    编辑器抽象 + Scintilla 实现
src/preview/   预览抽象 + IE 实现（插件式，未来加引擎）
src/app/       WTL 主程序
tests/         GoogleTest 单元测试
```

## 构建要求

- Visual Studio 2022，安装「使用 C++ 的桌面开发」工作负载，**并勾选 ATL/MFC 组件**（WTL 依赖 ATL）。
- CMake ≥ 3.24。
- vcpkg（本机位于 `F:\vc_lib\vcpkg`；如不同，请修改 `CMakePresets.json` 中的
  `CMAKE_TOOLCHAIN_FILE`，或设置环境变量 `VCPKG_ROOT` 后改用 `$env{VCPKG_ROOT}`）。

## 构建

```powershell
cmake --preset vs2022
cmake --build --preset vs2022 --config Release
ctest --preset vs2022
```

产物位于 `build/vs2022/src/app/Release/ActiveXWebMark.exe`。

## CI/CD 与安装包

仓库包含两套 GitHub Actions 工作流：

- `.github/workflows/ci.yml`：push / PR 时在 Windows 上构建并跑单元测试。
- `.github/workflows/release.yml`：push `v*` tag 时构建，并同时生成两份安装程序：
  - WiX MSI —— `dist/ActiveXWebMark-<version>.msi`
  - NSIS Setup —— `dist/ActiveXWebMark-<version>-setup.exe`

安装包脚本位于 `packaging/`：`wix/ActiveXWebMark.wxs`（WiX v5）、
`nsis/ActiveXWebMark.nsi`（NSIS）。CI 通过 `vcpkg-configuration.json` 里声明的
overlay ports（`vcpkg-ports/`）复现本机的 discount `/J` 与 Scintilla 静态化补丁。

> 运行库依赖：程序以动态 CRT（`/MD`）链接，运行需要
> 「Microsoft Visual C++ 2015-2022 Redistributable (x64)」，绝大多数 Windows 10/11
> 已自带。NSIS 安装时如检测不到会给出提示。

## 发布

```powershell
git tag v0.1.0
git push origin v0.1.0
```

推送 tag 后 GitHub Actions 自动构建、跑测试并生成 MSI 与 Setup.exe，作为资产附在
对应的 GitHub Release 上。tag 建议使用 `v<major>.<minor>.<patch>` 三段格式。

## 开发协作

分支模型：`main`（稳定/发布）← `dev`（开发集成）← `feature/*` / `fix/*`。
贡献与发布流程见 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 本地生成安装包

先构建程序，再用 WiX / NSIS 打包，产物输出到 `dist/`。

```powershell
# 0. 构建（在 build/vs2022/src/app/Release/ 下生成 ActiveXWebMark.exe）
cmake --build --preset vs2022 --config Release

# 1. NSIS（安装 NSIS 后，从仓库根目录执行）
makensis /DVERSION=0.1.0 packaging\nsis\ActiveXWebMark.nsi

# 2. WiX（安装 WiX 后，从仓库根目录执行）
dotnet tool install --global wix      # 仅首次
$env:WIX_VERSION = "0.1.0"
$env:AWM_EXE = (Resolve-Path "build\vs2022\src\app\Release\ActiveXWebMark.exe").Path
wix build -o "dist\ActiveXWebMark-0.1.0.msi" packaging\wix\ActiveXWebMark.wxs
```

产物：
- `dist\ActiveXWebMark-0.1.0-setup.exe`（NSIS）
- `dist\ActiveXWebMark-0.1.0.msi`（WiX）

## 许可证

见各源文件头。第三方组件（discount：BSD-3；PMH：MIT；Scintilla：MIT；WTL：MS-PL）
分别遵循其自身许可证。
