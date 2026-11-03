#requires -Version 5.1
<#
.SYNOPSIS
    Manual smoke test for the preview find bar (AwmFindBar).

.DESCRIPTION
    Launches the built application with a generated Markdown document and checks:
      * Ctrl+F opens the find bar and focusing/searching works;
      * incremental search, match-case, whole-word and regex options;
      * next / previous navigation, wrapping and the F3 accelerator;
      * invalid regular expressions report an error instead of throwing;
      * Esc closes the bar.

    The active DocumentView exposes a diagnostic message (WM_APP + 0x52) that
    returns only integers, so this test works across processes without pointer
    marshaling:
      wParam 0 -> 1 when the find bar is visible
      wParam 1 -> match count
      wParam 2 -> current match index (0-based, -1 when none)
      wParam 3 -> result code (1 empty query, 2 unsupported, 3 empty result,
                  4 invalid regex, 5 parse error, 6 ok)

    The app runs with AWM_SETTINGS_DIR pointing into -OutDir, so the user's real
    settings, session and recent files are untouched. Screenshots are saved to
    -OutDir for inspection.

    This is a manual smoke test, not part of CI. Run it from PowerShell:
        pwsh -File tests/manual/find-smoke.ps1
        powershell -ExecutionPolicy Bypass -File tests/manual/find-smoke.ps1

.PARAMETER Exe
    Path to ActiveXWebMark.exe. Defaults to build\vs2022\src\app\Release.

.PARAMETER OutDir
    Artifact folder. Defaults to %TEMP%\awm-find-smoke.

.PARAMETER TimeoutSeconds
    Maximum time to wait for the app to become responsive.

.NOTES
    调用流程：
      1. 校验参数，定位 ActiveXWebMark.exe，准备输出目录和测试 Markdown 文件
      2. 设置 AWM_SETTINGS_DIR，把被测程序的设置/会话隔离到输出目录
      3. 启动程序，按 PID 查找主窗口、可见的 AwmDocumentView 和查找栏
      4. 通过 WM_SETTEXT 输入查询，用诊断消息读取匹配数/当前索引
      5. 依次验证大小写、全词、正则、F3 导航和 Esc 关闭
      6. finally 中发送 WM_CLOSE 优雅退出，超时则强制结束进程，并恢复环境变量
      7. 最后输出 PASS/FAIL，并用 exit 0/exit 1 返回测试结果

    使用 SendMessage/PostMessage 而不是真实键盘输入：不抢焦点、不移动用户鼠标。
#>
[CmdletBinding()]
param(
    [string]$Exe = "",
    [string]$OutDir = "",
    [int]$TimeoutSeconds = 30
)

$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------------------
# 1. 路径与参数校验
# ---------------------------------------------------------------------------
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $Exe) {
    $Exe = Join-Path $repoRoot "build\vs2022\src\app\Release\ActiveXWebMark.exe"
}
if (-not (Test-Path -LiteralPath $Exe)) {
    throw "找不到程序：$Exe（请先构建 Release，或用 -Exe 指定路径）"
}
$Exe = (Resolve-Path -LiteralPath $Exe).Path
if ($TimeoutSeconds -lt 5) {
    $TimeoutSeconds = 5
}

# ---------------------------------------------------------------------------
# 2. 输出目录与测试文档
# ---------------------------------------------------------------------------
if (-not $OutDir) {
    $OutDir = Join-Path $env:TEMP "awm-find-smoke"
}
$OutDir = [System.IO.Path]::GetFullPath($OutDir)
if (Test-Path -LiteralPath $OutDir) {
    Remove-Item -LiteralPath $OutDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$document = Join-Path $OutDir "Find.md"
@"
# 查找测试

Hello world. hello HELLO.

数字 123 和 456 在这里。
"@ | Set-Content -LiteralPath $document -Encoding utf8

$previousSettingsDir = $env:AWM_SETTINGS_DIR
$env:AWM_SETTINGS_DIR = Join-Path $OutDir "settings"
$previousUpdateCheck = $env:AWM_DISABLE_UPDATE_CHECK
$env:AWM_DISABLE_UPDATE_CHECK = "1"

# ---------------------------------------------------------------------------
# 3. Win32 互操作助手
# ---------------------------------------------------------------------------
Add-Type -AssemblyName System.Drawing
if (-not ([System.Management.Automation.PSTypeName]'AwmFindSmoke').Type) {
    Add-Type -TypeDefinition @"
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class AwmFindSmoke
{
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc cb, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr parent, EnumWindowsProc cb, IntPtr lParam);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassNameW(IntPtr hWnd, StringBuilder sb, int n);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindowExW(IntPtr parent, IntPtr after, string cls, string title);
    [DllImport("user32.dll")]
    public static extern IntPtr SendMessageW(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr SendMessageW(IntPtr hWnd, uint msg, IntPtr wParam, StringBuilder lParam);
    [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr hDlg, int id);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdc, uint flags);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int L, T, R, B; }

    public static IntPtr FindClass(uint pid, string cls)
    {
        IntPtr result = IntPtr.Zero;
        EnumWindows((h, l) =>
        {
            uint p;
            GetWindowThreadProcessId(h, out p);
            if (p == pid)
            {
                var c = new StringBuilder(256);
                GetClassNameW(h, c, 256);
                if (c.ToString() == cls) { result = h; return false; }
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }

    public static IntPtr FindVisibleChildClass(IntPtr parent, string cls)
    {
        IntPtr result = IntPtr.Zero;
        EnumChildWindows(parent, (h, l) =>
        {
            var c = new StringBuilder(256);
            GetClassNameW(h, c, 256);
            if (c.ToString() == cls && IsWindowVisible(h)) { result = h; return false; }
            return true;
        }, IntPtr.Zero);
        return result;
    }
}
"@
}

# DocumentView::kTestFindStateMessage。
$stateMessage = 0x8052

# ---------------------------------------------------------------------------
# 4. 断言与操作助手
# ---------------------------------------------------------------------------
$script:failures = New-Object System.Collections.Generic.List[string]

function Check([bool]$condition, [string]$message) {
    if ($condition) {
        Write-Host "  PASS  $message" -ForegroundColor Green
    } else {
        Write-Host "  FAIL  $message" -ForegroundColor Red
        [void]$script:failures.Add($message)
    }
}

# 读取查找状态：wParam 0/1/2/3 分别返回可见、匹配数、当前索引、结果码。
function Get-FindState([IntPtr]$view, [int]$what) {
    return [AwmFindSmoke]::SendMessageW(
        $view, $stateMessage, [IntPtr]$what, [IntPtr]::Zero).ToInt64()
}

# WM_SETTEXT 会像真实输入一样触发 EN_CHANGE，查找栏随即（防抖后）执行搜索。
function Set-Query([IntPtr]$edit, [string]$text) {
    $ptr = [System.Runtime.InteropServices.Marshal]::StringToHGlobalUni($text)
    try {
        [void][AwmFindSmoke]::SendMessageW($edit, 0x000C, [IntPtr]::Zero, $ptr)
    } finally {
        [System.Runtime.InteropServices.Marshal]::FreeHGlobal($ptr)
    }
    Start-Sleep -Milliseconds 400
}

function Read-EditText([IntPtr]$edit) {
    $sb = New-Object System.Text.StringBuilder 256
    [void][AwmFindSmoke]::SendMessageW($edit, 0x000D, [IntPtr]256, $sb)
    return $sb.ToString()
}

# 点击查找栏按钮（选项开关、上一个/下一个、关闭）。
function Click-Button([IntPtr]$findBar, [int]$id) {
    $button = [AwmFindSmoke]::GetDlgItem($findBar, $id)
    [void][AwmFindSmoke]::SendMessageW($button, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) # BM_CLICK
    Start-Sleep -Milliseconds 350
}

function Save-Bitmap([IntPtr]$hwnd, [string]$path) {
    $rect = New-Object AwmFindSmoke+RECT
    [void][AwmFindSmoke]::GetClientRect($hwnd, [ref]$rect)
    $width = [Math]::Max(1, $rect.R - $rect.L)
    $height = [Math]::Max(1, $rect.B - $rect.T)
    $bitmap = New-Object System.Drawing.Bitmap $width, $height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $hdc = $graphics.GetHdc()
    [void][AwmFindSmoke]::PrintWindow($hwnd, $hdc, 1)
    $graphics.ReleaseHdc($hdc)
    $graphics.Dispose()
    $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bitmap.Dispose()
}

# ---------------------------------------------------------------------------
# 5. 主流程
# ---------------------------------------------------------------------------
$proc = $null
$main = [IntPtr]::Zero

try {
    $proc = Start-Process -FilePath $Exe -ArgumentList ('"' + $document + '"') -PassThru

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds 250
        $proc.Refresh()
        if ($proc.HasExited) {
            break
        }
        $main = [AwmFindSmoke]::FindClass([uint32]$proc.Id, "ActiveXWebMarkMainWindow")
        if ($main -ne [IntPtr]::Zero) {
            break
        }
    }
    if ($main -eq [IntPtr]::Zero) {
        throw "主窗口未出现（${TimeoutSeconds} 秒）"
    }
    Start-Sleep -Milliseconds 1500

    $view = [AwmFindSmoke]::FindVisibleChildClass($main, "AwmDocumentView")
    if ($view -eq [IntPtr]::Zero) {
        throw "找不到可见的 AwmDocumentView 子窗口"
    }
    Check ((Get-FindState $view 0) -eq 0) "启动时查找栏隐藏"

    [void][AwmFindSmoke]::SendMessageW($main, 0x0111, [IntPtr]1020, [IntPtr]::Zero) # kEditFind
    Start-Sleep -Milliseconds 500
    $findBar = [AwmFindSmoke]::FindWindowExW($view, [IntPtr]::Zero, "AwmFindBar", $null)
    if ($findBar -eq [IntPtr]::Zero) {
        throw "找不到 AwmFindBar 子窗口"
    }
    $edit = [AwmFindSmoke]::FindWindowExW($findBar, [IntPtr]::Zero, "Edit", $null)
    if ($edit -eq [IntPtr]::Zero) {
        throw "找不到查找输入框"
    }
    Check ((Get-FindState $view 0) -eq 1) "Ctrl+F 打开查找栏"

    # 5.0 Backspace 回归测试：主窗口转发加速键时必须按焦点过滤，否则
    #     IE 预览会吞掉查找框的 VK_BACK（表现为无法删除字符）。
    Set-Query $edit ""
    foreach ($ch in @(0x68, 0x65, 0x6C, 0x6C, 0x6F)) {
        [void][AwmFindSmoke]::PostMessageW($edit, 0x0102, [IntPtr]$ch, [IntPtr]::Zero)
        Start-Sleep -Milliseconds 150
    }
    Check ((Read-EditText $edit) -eq "hello") "键盘输入写入查找框"
    [void][AwmFindSmoke]::PostMessageW(
        $edit, 0x0100, [IntPtr]0x08, [IntPtr]0x000E0001) # VK_BACK
    Start-Sleep -Milliseconds 300
    Check ((Read-EditText $edit) -eq "hell") "Backspace 能删除最后一个字符"

    # 5.1 默认不区分大小写，三项都能命中。
    Set-Query $edit "hello"
    Check ((Get-FindState $view 1) -eq 3) "默认不区分大小写：hello 命中 3 处"
    Check ((Get-FindState $view 2) -eq 0) "初始定位到第 1 处"

    [void][AwmFindSmoke]::SendMessageW($main, 0x0111, [IntPtr]1021, [IntPtr]::Zero) # kEditFindNext
    Start-Sleep -Milliseconds 300
    Check ((Get-FindState $view 2) -eq 1) "查找下一个前进到第 2 处"

    # 5.2 输入框里的 Enter 由查找栏自己处理；F3 由主窗口的
    #     PreTranslateMessage 处理，PostMessage 会走正常消息循环。
    [void][AwmFindSmoke]::PostMessageW($edit, 0x0100, [IntPtr]0x0D, [IntPtr]::Zero) # VK_RETURN
    Start-Sleep -Milliseconds 350
    Check ((Get-FindState $view 2) -eq 2) "Enter 前进到第 3 处"
    [void][AwmFindSmoke]::PostMessageW($edit, 0x0100, [IntPtr]0x72, [IntPtr]::Zero) # VK_F3
    Start-Sleep -Milliseconds 350
    Check ((Get-FindState $view 2) -eq 0) "F3 在末尾回绕到第 1 处"

    # 5.3 全词匹配：hell 不是独立的词，打开后命中数变为 0。
    Set-Query $edit "hell"
    Check ((Get-FindState $view 1) -eq 3) "关闭全词：hell 命中 3 处"
    Click-Button $findBar 3
    Check ((Get-FindState $view 1) -eq 0) "开启全词：hell 不再命中 hello"
    Click-Button $findBar 3
    Save-Bitmap $findBar (Join-Path $OutDir "01-findbar.png")
    Save-Bitmap $main (Join-Path $OutDir "02-highlights.png")

    # 5.4 区分大小写：只有小写 hello 命中。
    Set-Query $edit "hello"
    Click-Button $findBar 2
    Check ((Get-FindState $view 1) -eq 1) "区分大小写：只有小写 hello 命中"
    Click-Button $findBar 2
    Set-Query $edit "world"
    Check ((Get-FindState $view 1) -eq 1) "普通文本查找 world 命中 1 处"

    # 5.5 正则：开启前 \d+ 作为字面量无匹配，开启后命中两个数字。
    Set-Query $edit "\d+"
    Check ((Get-FindState $view 1) -eq 0) "未开启正则：\\d+ 无匹配"
    Click-Button $findBar 4
    Check ((Get-FindState $view 1) -eq 2) "开启正则：\\d+ 命中 2 个数字"
    Check ((Get-FindState $view 3) -eq 6) "正则搜索成功返回结果码"

    # 5.6 非法正则：不崩溃，返回错误码且匹配数为 0。
    Set-Query $edit "["
    Check ((Get-FindState $view 1) -eq 0) "非法正则不产生匹配"
    Check ((Get-FindState $view 3) -eq 4) "非法正则返回错误码"

    # 5.7 Esc 关闭查找栏并清除高亮。
    [void][AwmFindSmoke]::PostMessageW($edit, 0x0100, [IntPtr]0x1B, [IntPtr]::Zero) # VK_ESCAPE
    Start-Sleep -Milliseconds 500
    Check ((Get-FindState $view 0) -eq 0) "Esc 关闭查找栏"
}
catch {
    Write-Host "ERROR  $($_.Exception.Message)" -ForegroundColor Red
    [void]$script:failures.Add($_.Exception.Message)
}
finally {
    if ($proc -ne $null) {
        $proc.Refresh()
        if (-not $proc.HasExited) {
            if ($main -ne [IntPtr]::Zero) {
                [void][AwmFindSmoke]::PostMessageW(
                    $main, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
            }
            for ($i = 0; $i -lt 20; $i++) {
                Start-Sleep -Milliseconds 200
                $proc.Refresh()
                if ($proc.HasExited) {
                    break
                }
            }
            if (-not $proc.HasExited) {
                Stop-Process -Id $proc.Id -Force
            }
        }
    }

    if ($null -eq $previousSettingsDir) {
        Remove-Item Env:AWM_SETTINGS_DIR -ErrorAction SilentlyContinue
    } else {
        $env:AWM_SETTINGS_DIR = $previousSettingsDir
    }
    if ($null -eq $previousUpdateCheck) {
        Remove-Item Env:AWM_DISABLE_UPDATE_CHECK -ErrorAction SilentlyContinue
    } else {
        $env:AWM_DISABLE_UPDATE_CHECK = $previousUpdateCheck
    }
}

Write-Host ""
Write-Host "截图目录：$OutDir" -ForegroundColor Cyan
if ($script:failures.Count -eq 0) {
    Write-Host "SMOKE PASS" -ForegroundColor Green
    exit 0
}
Write-Host ("SMOKE FAIL ({0} 项)" -f $script:failures.Count) -ForegroundColor Red
foreach ($failure in $script:failures) {
    Write-Host "  - $failure" -ForegroundColor Red
}
exit 1
