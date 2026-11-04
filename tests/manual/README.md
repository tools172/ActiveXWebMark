# 手动冒烟测试

这些脚本用于人工检查无法稳定放进 CI 的 Win32 界面行为。

## tabstrip-smoke.ps1

检查自绘标签栏（`AwmTabStrip`）：

- 标签溢出时左右滚动箭头能到达末尾并回到开头（同时能发现两个箭头逻辑互换）
- 目录开关按钮能切换目录显示/隐藏（通过 tooltip 文本断言）
- 各步骤截图保存到输出目录，便于人工复核

运行（需先构建 Release）：

```powershell
pwsh -File tests/manual/tabstrip-smoke.ps1
```

如果系统阻止执行脚本：

```powershell
powershell -ExecutionPolicy Bypass -File tests/manual/tabstrip-smoke.ps1
```

脚本保存为 **UTF-8 with BOM**，Windows PowerShell 5.1 和 PowerShell 7 都能直接解析中文；
后续编辑时请保留 BOM，否则 5.1 会按本地 ANSI 编码读取并出现乱码/解析错误。

可选参数：

```text
-Exe <path>           指定 ActiveXWebMark.exe，默认 build\vs2022\src\app\Release
-TabCount <n>         生成标签数，默认 16（少于 10 可能不溢出）
-OutDir <path>        截图和测试文件目录，默认 %TEMP%\awm-tabstrip-smoke
-TimeoutSeconds <n>   等待窗口响应的最长时间，默认 30
```

脚本启动的程序使用 `AWM_SETTINGS_DIR` 指向输出目录下的隔离设置文件夹，不会读取或修改
`%LOCALAPPDATA%\ActiveXWebMark\settings.json`，因此可以和已打开的 ActiveXWebMark 同时运行，
也不会改变你的标签会话、最近文件或窗口布局。

所有冒烟脚本都会设置 `AWM_DISABLE_UPDATE_CHECK=1`，关闭启动时的后台更新检测，避免更新提示
打扰自动化步骤。手动验证更新功能时请不要设置该变量。

## titlebar-smoke.ps1

检查自绘标题栏与自定义窗口框架（`MainWindow` + `ui::TitleBar`）：

- 原生菜单栏已移除，`GetMenu` 返回 `NULL`；标签栏紧贴标题栏下方
- 标题栏依次排列应用图标、文件/编辑/视图/帮助、窗口标题、最小化/最大化/关闭，
  布局在不同 DPI 下按比例缩放
- `WM_NCHITTEST`：菜单和按钮返回 `HTCLIENT`，空白标题区返回 `HTCAPTION`，
  四边/四角返回缩放命中码；Windows 11 上最大化按钮返回 `HTMAXBUTTON`
- 悬停高亮：菜单标签、最小化/最大化/关闭按钮（通过诊断消息注入，避免真实光标竞态）
- 文件/视图下拉菜单由鼠标和 Alt+F 打开，保留原生菜单项、勾选状态与分隔线，Esc 关闭；
  选择「滚动同步」时会短暂移动真实光标点击菜单项，随后恢复原光标位置
- F10 → 左右方向键 → 下方向键的键盘菜单导航
- 点击应用图标打开系统菜单
- 最大化/还原后客户区等于显示器工作区（任务栏可见）、最小化/恢复、关闭按钮退出

运行：

```powershell
pwsh -File tests/manual/titlebar-smoke.ps1
```

可选参数：`-Exe`、`-OutDir`、`-TimeoutSeconds`。脚本使用隔离的
`AWM_SETTINGS_DIR`，不会影响真实设置或会话；截图保存到输出目录。

## find-smoke.ps1

检查预览查找栏（`AwmFindBar`）：

- Ctrl+F 打开/关闭查找栏，输入后实时高亮匹配并在预览中滚动到当前项
- 区分大小写、全词匹配、正则表达式三个选项
- 查找下一个 / F3（含末尾回绕）与非法正则的错误处理
- Esc 关闭查找栏并清除高亮

运行方式与 `tabstrip-smoke.ps1` 相同：

```powershell
pwsh -File tests/manual/find-smoke.ps1
```

可选参数：`-Exe`、`-OutDir`、`-TimeoutSeconds`。输出目录中会保存查找栏和主窗口截图。
