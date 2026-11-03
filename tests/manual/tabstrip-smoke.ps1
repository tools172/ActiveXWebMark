#requires -Version 5.1
<#
.SYNOPSIS
    Manual smoke test for the owner-drawn tab strip (AwmTabStrip).

.DESCRIPTION
    Launches the built application with generated Markdown tabs and exercises:
      * tab overflow: clicking > repeatedly must reach the end, then < must
        return to the start (this also catches swapped arrow handlers);
      * sidebar toggle: the nav-visible state must flip on each click.

    The tab strip exposes a diagnostic message (WM_APP + 0x51) returning only
    integers, so this test works across processes without pointer marshaling:
      wParam 0 -> flags: bit0 overflowLeft, bit1 overflowRight, bit2 navVisible
      wParam 1 -> firstVisible index
      wParam 2 -> maxFirstVisible index

    The app is launched with AWM_SETTINGS_DIR pointing into -OutDir, so the
    user's real settings, session and recent files are not touched. Generated
    tabs, settings and screenshots stay in -OutDir for inspection.

    This is a manual smoke test, not part of CI. Run it from PowerShell:
        pwsh -File tests/manual/tabstrip-smoke.ps1
        powershell -ExecutionPolicy Bypass -File tests/manual/tabstrip-smoke.ps1

.PARAMETER Exe
    Path to ActiveXWebMark.exe. Defaults to build\vs2022\src\app\Release.

.PARAMETER TabCount
    Number of generated tabs. Use 16 or more so the strip overflows.

.PARAMETER OutDir
    Artifact folder. Defaults to %TEMP%\awm-tabstrip-smoke.

.PARAMETER TimeoutSeconds
    Maximum time to wait for the app to become responsive.

.NOTES
    调用流程：
      1. 校验参数，定位 ActiveXWebMark.exe，准备输出目录和测试 Markdown 文件
      2. 设置 AWM_SETTINGS_DIR，把被测程序的设置/会话隔离到输出目录
      3. Start-Process 启动被测程序，按 PID 查找主窗口和 AwmTabStrip 子窗口
      4. 读取标签栏状态和按钮矩形，执行滚动箭头边界测试与目录开关测试
      5. 每一步用 PrintWindow 截图，失败信息汇总到 $script:failures
      6. finally 中发送 WM_CLOSE 优雅退出，超时则强制结束进程，并恢复环境变量
      7. 最后输出 PASS/FAIL，并用 exit 0/exit 1 返回测试结果

    之所以用 SendMessage 而不是真实鼠标：不抢焦点、不移动用户鼠标，
    在普通 PowerShell 窗口里也能稳定复现。按钮坐标按 DPI 和控件客户端尺寸计算。
#>
[CmdletBinding()]
param(
    [string]$Exe = "",
    [int]$TabCount = 16,
    [string]$OutDir = "",
    [int]$TimeoutSeconds = 30
)

$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------------------
# 1. 路径与参数校验
#    任何错误都尽早抛出，避免脚本启动到一半才发现路径或参数不对。
# ---------------------------------------------------------------------------
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $Exe) {
    $Exe = Join-Path $repoRoot "build\vs2022\src\app\Release\ActiveXWebMark.exe"
}
if (-not (Test-Path -LiteralPath $Exe)) {
    throw "找不到程序：$Exe（请先构建 Release，或用 -Exe 指定路径）"
}
$Exe = (Resolve-Path -LiteralPath $Exe).Path

if ($TabCount -lt 1 -or $TabCount -gt 100) {
    throw "-TabCount 必须在 1 到 100 之间"
}
if ($TimeoutSeconds -lt 5) {
    $TimeoutSeconds = 5
}

# ---------------------------------------------------------------------------
# 2. 输出目录与测试文件
#    每次运行清空输出目录；截图、隔离设置和生成的 Markdown 都放在这里。
# ---------------------------------------------------------------------------
if (-not $OutDir) {
    $OutDir = Join-Path $env:TEMP "awm-tabstrip-smoke"
}
$OutDir = [System.IO.Path]::GetFullPath($OutDir)
if (Test-Path -LiteralPath $OutDir) {
    Remove-Item -LiteralPath $OutDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$tabDir = Join-Path $OutDir "tabs"
New-Item -ItemType Directory -Force -Path $tabDir | Out-Null

# ---------------------------------------------------------------------------
# 3. 隔离程序设置
#    被测程序支持 AWM_SETTINGS_DIR；指向输出目录后，本次启动不会读写
#    %LOCALAPPDATA% 下的真实设置，因此可以与已运行的实例并存。
#    finally 中会恢复原环境变量。
# ---------------------------------------------------------------------------
$previousSettingsDir = $env:AWM_SETTINGS_DIR
$env:AWM_SETTINGS_DIR = Join-Path $OutDir "settings"

# ---------------------------------------------------------------------------
# 4. Win32 互操作助手
#    只使用窗口句柄、整数消息和截图 API；不向目标进程传指针，
#    因为跨进程 SendMessage 不会替我们封送 TOOLINFO 之类的结构体。
# ---------------------------------------------------------------------------
Add-Type -AssemblyName System.Drawing
if (-not ([System.Management.Automation.PSTypeName]'AwmTabStripSmoke').Type) {
    Add-Type -TypeDefinition @"
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class AwmTabStripSmoke
{
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    // 窗口枚举、查找、消息、矩形、截图、DPI。
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc cb, IntPtr lParam);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassNameW(IntPtr hWnd, StringBuilder sb, int n);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindowExW(IntPtr parent, IntPtr after, string cls, string title);
    [DllImport("user32.dll")]
    public static extern IntPtr SendMessageW(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")]
    public static extern bool PostMessageW(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")]
    public static extern IntPtr SendMessageTimeoutW(IntPtr hWnd, uint msg, IntPtr wParam,
        IntPtr lParam, uint flags, uint timeout, out IntPtr result);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr hWnd);

    // 与 Win32 RECT 的字段布局保持一致。
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int L, T, R, B; }

    // 按进程 ID + 窗口类名查找窗口。不能依赖窗口标题，因为标题会随文档变化。
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
                if (c.ToString() == cls)
                {
                    result = h;
                    return false;
                }
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }

    // 用 WM_NULL + SendMessageTimeout 探测 UI 线程是否在 timeoutMs 内响应。
    // 大文件渲染期间会暂时不响应，所以等待窗口时要反复调用。
    public static bool Responsive(IntPtr hWnd, uint timeoutMs)
    {
        IntPtr result;
        return SendMessageTimeoutW(hWnd, 0x0000, IntPtr.Zero, IntPtr.Zero,
            0x0002 /* SMTO_ABORTIFHUNG */, timeoutMs, out result) != IntPtr.Zero;
    }
}
"@
}

# 被测控件在 tabstrip.h 中定义的诊断消息：TabStrip::kTestStateMessage。
#   wParam=0 -> flags：bit0 overflowLeft，bit1 overflowRight，bit2 navVisible
#   wParam=1 -> firstVisible 索引
#   wParam=2 -> maxFirstVisible 索引
$stateMessage = 0x8051

# ---------------------------------------------------------------------------
# 5. 断言、等待、状态读取、几何计算、点击与截图工具
# ---------------------------------------------------------------------------
$script:failures = New-Object System.Collections.Generic.List[string]

# 记录一条断言结果；失败时同时打印并加入最终失败列表。
function Check([bool]$condition, [string]$message) {
    if ($condition) {
        Write-Host "  PASS  $message" -ForegroundColor Green
    } else {
        Write-Host "  FAIL  $message" -ForegroundColor Red
        [void]$script:failures.Add($message)
    }
}

# 矩形宽度（右边界 - 左边界）；宽度为 0 表示该按钮当前隐藏。
function Rect-Width($rect) {
    return [int]($rect.R - $rect.L)
}

# 轮询被测进程的主窗口：找到窗口且 UI 线程在 100ms 内响应才返回。
# 这样不会在 OpenFiles / RenderNow 还没结束时读取控件状态。
function Wait-MainWindow([System.Diagnostics.Process]$proc, [int]$timeoutSeconds) {
    $deadline = [DateTime]::UtcNow.AddSeconds($timeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds 200
        $proc.Refresh()
        if ($proc.HasExited) {
            return [IntPtr]::Zero
        }
        $h = [AwmTabStripSmoke]::FindClass([uint32]$proc.Id, "ActiveXWebMarkMainWindow")
        if ($h -ne [IntPtr]::Zero -and [AwmTabStripSmoke]::Responsive($h, 100)) {
            return $h
        }
    }
    return [IntPtr]::Zero
}

# 主窗口出现后，标签栏子窗口可能在布局过程中才创建，短暂轮询即可。
function Wait-TabStrip([IntPtr]$main, [int]$timeoutSeconds) {
    $deadline = [DateTime]::UtcNow.AddSeconds($timeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        $strip = [AwmTabStripSmoke]::FindWindowExW($main, [IntPtr]::Zero, "AwmTabStrip", $null)
        if ($strip -ne [IntPtr]::Zero) {
            return $strip
        }
        Start-Sleep -Milliseconds 150
    }
    return [IntPtr]::Zero
}

# 读取标签栏状态：连续发送三次诊断消息，分别取 flags、firstVisible、maxFirstVisible。
# 只返回值，不跨进程传指针，因此对 x86/x64 检测进程都安全。
function Get-StripState([IntPtr]$strip) {
    $flags = [AwmTabStripSmoke]::SendMessageW(
        $strip, $stateMessage, [IntPtr]0, [IntPtr]::Zero).ToInt64()
    $first = [AwmTabStripSmoke]::SendMessageW(
        $strip, $stateMessage, [IntPtr]1, [IntPtr]::Zero).ToInt64()
    $max = [AwmTabStripSmoke]::SendMessageW(
        $strip, $stateMessage, [IntPtr]2, [IntPtr]::Zero).ToInt64()
    return [pscustomobject]@{ Flags = $flags; First = $first; Max = $max }
}

# 计算滚动箭头和目录按钮在标签栏客户区中的矩形。
# 尺寸必须与 tabstrip.cpp 里的 theme_->px() 保持一致；这里用
# GetDpiForWindow + 四舍五入（MulDiv 的等价算法）换算逻辑像素。
# 坐标约定和控件实现一致：左上角为 (0,0)，右/下边界为开区间。
function Get-StripGeometry([IntPtr]$strip) {
    $client = New-Object AwmTabStripSmoke+RECT
    [void][AwmTabStripSmoke]::GetClientRect($strip, [ref]$client)
    $dpi = [AwmTabStripSmoke]::GetDpiForWindow($strip)
    if ($dpi -le 0) {
        $dpi = 96
    }
    $arrow = [int][Math]::Floor((22 * $dpi / 96.0) + 0.5)
    $navSize = [int][Math]::Floor((24 * $dpi / 96.0) + 0.5)
    $margin = [int][Math]::Floor((8 * $dpi / 96.0) + 0.5)
    $gap = [int][Math]::Floor((8 * $dpi / 96.0) + 0.5)
    $top = [int][Math]::Floor((7 * $dpi / 96.0) + 0.5)
    $bottom = [Math]::Max($top, $client.B - 1)
    $navLeft = $client.R - $margin - $navSize
    $navTop = [Math]::Max(0, [int](($client.B - 1 - $navSize) / 2))
    $tabAreaRight = [Math]::Max(0, $navLeft - $gap)
    return [pscustomobject]@{
        LeftArrow = [pscustomobject]@{ L = 0; T = $top; R = $arrow; B = $bottom }
        RightArrow = [pscustomobject]@{
            L = [Math]::Max(0, $tabAreaRight - $arrow); T = $top; R = $tabAreaRight; B = $bottom
        }
        Nav = [pscustomobject]@{
            L = $navLeft; T = $navTop; R = $navLeft + $navSize; B = $navTop + $navSize
        }
    }
}

# 向标签栏发送一次左键按下/抬起。lParam 是客户端坐标（MAKELPARAM 格式），
# 直接调用控件的 OnLButtonDown，不移动真实鼠标，也不会抢走用户焦点。
function Click-Rect([IntPtr]$strip, $rect) {
    $width = Rect-Width $rect
    $height = [int]($rect.B - $rect.T)
    if ($width -le 0 -or $height -le 0) {
        return $false
    }
    $x = [int]($rect.L + $width / 2)
    $y = [int]($rect.T + $height / 2)
    $lp = [IntPtr][int](($y -shl 16) -bor ($x -band 0xffff))
    [void][AwmTabStripSmoke]::SendMessageW($strip, 0x0201, [IntPtr]1, $lp)
    [void][AwmTabStripSmoke]::SendMessageW($strip, 0x0202, [IntPtr]::Zero, $lp)
    Start-Sleep -Milliseconds 300
    return $true
}

# 用 PrintWindow 抓取窗口或客户区并保存为 PNG，供人工复核。
# clientOnly=true 用于标签栏；false 用于包含目录侧栏的主窗口。
function Save-WindowBitmap([IntPtr]$hwnd, [string]$path, [bool]$clientOnly) {
    $rect = New-Object AwmTabStripSmoke+RECT
    if ($clientOnly) {
        [void][AwmTabStripSmoke]::GetClientRect($hwnd, [ref]$rect)
    } else {
        [void][AwmTabStripSmoke]::GetWindowRect($hwnd, [ref]$rect)
    }
    $width = [Math]::Max(1, $rect.R - $rect.L)
    $height = [Math]::Max(1, $rect.B - $rect.T)
    $bitmap = New-Object System.Drawing.Bitmap $width, $height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $hdc = $graphics.GetHdc()
    [void][AwmTabStripSmoke]::PrintWindow($hwnd, $hdc, 2)
    $graphics.ReleaseHdc($hdc)
    $graphics.Dispose()
    $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bitmap.Dispose()
}

# ---------------------------------------------------------------------------
# 6. 主流程
#    try 负责启动、等待和测试；catch 记录异常；finally 负责退出进程和恢复环境。
# ---------------------------------------------------------------------------
$proc = $null
$main = [IntPtr]::Zero

try {
    # 6.1 生成 TabCount 个 Markdown 文件；排序后作为命令行参数一次传入。
    $files = @()
    for ($i = 1; $i -le $TabCount; $i++) {
        $file = Join-Path $tabDir ("Smoke{0:D2}.md" -f $i)
        Set-Content -LiteralPath $file -Value ("# Smoke {0:D2}`n`nTab {0}" -f $i) -Encoding utf8
        $files += $file
    }
    $quoted = @($files | ForEach-Object { '"' + $_ + '"' })

    # 6.2 启动被测进程。路径逐个加引号，避免临时目录含空格时被拆成多个参数。
    Write-Host "启动：$Exe" -ForegroundColor Cyan
    Write-Host "标签数：$TabCount    输出：$OutDir" -ForegroundColor Cyan
    $proc = Start-Process -FilePath $Exe -ArgumentList $quoted -PassThru

    $main = Wait-MainWindow $proc $TimeoutSeconds
    if ($main -eq [IntPtr]::Zero) {
        throw "主窗口未出现或未响应（${TimeoutSeconds} 秒）"
    }
    $strip = Wait-TabStrip $main 5
    if ($strip -eq [IntPtr]::Zero) {
        throw "找不到 AwmTabStrip 子窗口"
    }
    Start-Sleep -Milliseconds 800

    # 6.3 标签栏已稳定：读取初始状态并保存第一张截图。
    $geometry = Get-StripGeometry $strip
    $state = Get-StripState $strip
    Save-WindowBitmap $strip (Join-Path $OutDir "01-initial.png") $true

    Check ((($state.Flags -band 1) -eq 0)) "初始状态：左箭头隐藏"
    Check ((($state.Flags -band 2) -ne 0)) "初始状态：右箭头可见（标签溢出）"

    # 6.4 滚动箭头边界测试：
    #     从开头连续点 >，右箭头隐藏说明到达末尾；
    #     再连续点 <，左箭头隐藏说明回到开头。
    #     若两个按钮逻辑互换，右箭头在开头不会变化，循环耗尽后断言失败。
    if (($state.Flags -band 2) -ne 0) {
        $maxClicks = $TabCount + 4

        # Click > until the end. A swapped handler would leave the right arrow
        # visible forever at the start, so this also catches reversed buttons.
        for ($i = 0; $i -lt $maxClicks; $i++) {
            $state = Get-StripState $strip
            if (($state.Flags -band 2) -eq 0) {
                break
            }
            [void](Click-Rect $strip $geometry.RightArrow)
        }
        $state = Get-StripState $strip
        Check ((($state.Flags -band 2) -eq 0)) "连续点击右箭头后到达末尾（右箭头隐藏）"
        Check ((($state.Flags -band 1) -ne 0)) "末尾状态：左箭头可见"
        Check (($state.First -eq $state.Max)) "firstVisible 到达 maxFirstVisible"
        Save-WindowBitmap $strip (Join-Path $OutDir "02-at-end.png") $true

        # Click < until the start.
        for ($i = 0; $i -lt $maxClicks; $i++) {
            $state = Get-StripState $strip
            if (($state.Flags -band 1) -eq 0) {
                break
            }
            [void](Click-Rect $strip $geometry.LeftArrow)
        }
        $state = Get-StripState $strip
        Check ((($state.Flags -band 1) -eq 0)) "连续点击左箭头后回到开头（左箭头隐藏）"
        Check ((($state.Flags -band 2) -ne 0)) "开头状态：右箭头可见"
        Check (($state.First -eq 0)) "firstVisible 回到 0"
        Save-WindowBitmap $strip (Join-Path $OutDir "03-back-to-start.png") $true
    } else {
        Write-Warning "标签未溢出，跳过箭头测试。请使用更大的 -TabCount（默认 16）。"
    }

    # 6.5 目录开关测试：点击一次 navVisible 位应翻转，再点一次恢复。
    $state = Get-StripState $strip
    $navBefore = (($state.Flags -band 4) -ne 0)
    Save-WindowBitmap $main (Join-Path $OutDir "04-nav-visible.png") $false
    [void](Click-Rect $strip $geometry.Nav)
    Start-Sleep -Milliseconds 400
    $state = Get-StripState $strip
    $navAfter = (($state.Flags -band 4) -ne 0)
    Save-WindowBitmap $main (Join-Path $OutDir "05-nav-hidden.png") $false
    Check ($navAfter -ne $navBefore) "点击目录开关后状态翻转"
    [void](Click-Rect $strip $geometry.Nav)
    Start-Sleep -Milliseconds 400
    $state = Get-StripState $strip
    $navBack = (($state.Flags -band 4) -ne 0)
    Check ($navBack -eq $navBefore) "再次点击目录开关后恢复原状态"
}
catch {
    # 未预期异常（找不到窗口、控件通讯失败等）也计入失败，
    # 但不会跳过 finally 的清理。
    Write-Host "ERROR  $($_.Exception.Message)" -ForegroundColor Red
    [void]$script:failures.Add($_.Exception.Message)
}
finally {
    # -----------------------------------------------------------------------
    # 7. 清理：先发送 WM_CLOSE 让程序正常退出并保存隔离设置；
    #    等待约 4 秒仍未退出才强制结束，避免脚本永久卡住。
    #    然后恢复 AWM_SETTINGS_DIR；测试设置留在输出目录供排查。
    # -----------------------------------------------------------------------
    if ($proc -ne $null) {
        $proc.Refresh()
        if (-not $proc.HasExited) {
            if ($main -ne [IntPtr]::Zero) {
                [void][AwmTabStripSmoke]::PostMessageW(
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
}

# ---------------------------------------------------------------------------
# 8. 结果汇总：输出截图目录；退出码 0 表示全部通过，1 表示存在失败。
# ---------------------------------------------------------------------------
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
