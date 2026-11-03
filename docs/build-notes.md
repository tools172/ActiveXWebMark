# 构建环境说明

本机无法直接访问 github.com（连接被重置），因此 vcpkg 的引导与依赖下载需要按下面
的方式离线完成。以下内容针对当前这台机器（`F:\vc_lib`）记录，供复现和排查。

## 1. vcpkg 工作区

本机有两份 vcpkg：

- `F:\vc_lib\vcpkg` —— 2026-06 的完整 checkout，但缺少 `vcpkg.exe`（新版 vcpkg 不再
  从源码编译，需从 GitHub 下载工具二进制）。
- `F:\vc_lib\vcpkg_2023` —— 用 `git worktree` 从 `F:\vc_lib\vcpkg` 的提交
  `000d1bda1f...`（2023-12-08）切出的工作区，并复制了 VS 捆绑的
  `vcpkg.exe`（2023-12-12）。

原因：VS 捆绑的 vcpkg.exe（2023-12-12）与 2026 的 registry 不兼容（`vcpkgTools.xml`
在 2025 年 1 月被移除）。用 2023-12 的 worktree 与 2023-12 的 exe 配对，版本自洽，
且 registry 就在本地、无需联网。

`CMakePresets.json` 中的 `CMAKE_TOOLCHAIN_FILE` 与 `VCPKG_DOWNLOADS` 均指向
`F:\vc_lib\vcpkg_2023`。如果换机器或换路径，改这两处即可（也可改用 `$env{VCPKG_ROOT}`）。

## 2. 源码包离线预下载（codeload）

github.com 被墙，但 `codeload.github.com` 可达。因此把 vcpkg 需要的源码包预先下载到
`F:\vc_lib\vcpkg_2023\downloads\`，按 portfile 里的文件名与 SHA512 命名，vcpkg 命中
缓存即跳过网络。

当前已缓存（SHA512 均已与 portfile 校验一致）：

- `Orc-discount-v3.0.0a.tar.gz`
- `nlohmann-json-v3.11.3.tar.gz`
- `fmtlib-fmt-10.1.1.tar.gz`
- `google-googletest-v1.14.0.tar.gz`
- `scintilla446.zip`
- `meson-bb91cea0d66d8d036063dedec1f194d663399cdf.tar.gz`
- `pkgconf-pkgconf-pkgconf-2.1.0.tar.gz`

下载方式示例（github 源经 codeload）：

```bash
curl -L -o "Orc-discount-v3.0.0a.tar.gz" \
  "https://codeload.github.com/Orc/discount/tar.gz/v3.0.0a"
```

## 3. portfile 补丁（在本 vcpkg worktree 内）

为了完全离线、免 MSYS2、并拿到静态 Scintilla，对 `F:\vc_lib\vcpkg_2023\ports\` 做了两处
改动：

1. **`fmt/gtest/nlohmann-json/discount` 的 `vcpkg_fixup_pkgconfig()` 改为
   `vcpkg_fixup_pkgconfig(SKIP_CHECK)`** —— 跳过 pkg-config 校验（该步需要下载 MSYS2）。
   `.pc` 文件的前缀修正仍会执行，不影响链接。

2. **`scintilla/portfile.cmake` 强制应用 `0001-static-lib.patch`**（把
   `if (VCPKG_LIBRARY_LINKAGE STREQUAL "static")` 改为 `if (TRUE)`）—— 因为
   `x64-windows` 三元组默认动态链接，而动态的 SciLexer.dll 只导出
   `Scintilla_DirectFunction`，不导出 `Scintilla_RegisterClasses`；静态库才有后者，
   这是创建 Scintilla 窗口类所需的入口。

3. **`discount/portfile.cmake` 增加 `0003-unsigned-char.patch`** —— 给 discount 的 C
   编译加 `/J`（`char` 按无符号处理）。MSVC 下 `char` 默认有符号，discount 里
   `isspace()/isalpha()` 直接作用于字节，遇到 UTF-8 中文（字节 ≥ 0x80，作为有符号
   `char` 是负数）时在 Debug CRT 触发断言（`__debugbreak`）。`/J` 让字节落在
   0..255，断言消除、行为也正确。

> 注意：这些是对 worktree 的本地修改，`git` 会显示 dirty。若重新切 worktree 或更新
> vcpkg，需要重新应用。

## 4. 其它

- 需要 VS2022 安装「使用 C++ 的桌面开发」工作负载，**并勾选 ATL/MFC**（WTL 依赖 ATL）。
- `third_party/wtl` 来自本机已有的 WTL 10.0.10320；`third_party/peg-markdown-highlight`
  复制自 CuteMarkEd 的 `3rdparty/peg-markdown-highlight`（`pmh_parser.cpp` 已重命名为
  `pmh_parser.c` 以便按 C 编译）。
- 0.2.0 起界面使用 GDI+ 自绘控件（`src/ui`），并通过
  `#pragma comment(linker, "/manifestdependency:...Common-Controls 6.0...")`
  启用 Common Controls v6（现代原生控件外观与 TaskDialog），无需额外的 `.rc`/manifest
  文件；DPI 感知在 `WinMain` 中通过
  `SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)` 设置。
