#requires -Version 5.1
<#
.SYNOPSIS
    Manual smoke test for the custom title bar / frameless main window.

.DESCRIPTION
    Launches the built application with a generated Markdown document and checks:
      * the native menu bar is gone and the top-level menus live in the title bar;
      * layout invariants: icon, menu labels, title and window buttons do not
        overlap and are ordered left-to-right;
      * tab strip starts exactly below the title bar;
      * WM_NCHITTEST returns HTCAPTION for drag areas, HTCLIENT for menu labels
        and window buttons, resize codes for the outer border, and HTMAXBUTTON
        on Windows 11;
      * hover feedback for menu labels and the minimize/maximize/close buttons;
      * the 文件/视图 popup menus open from mouse clicks and Alt+F, keep their
        native items/check states, and close with Escape; selecting the view
        menu's 滚动同步 item uses a short real mouse click (the cursor position
        is restored afterwards);
      * F10 / Left / Right / Down keyboard menu navigation;
      * the application icon opens the system menu;
      * minimize, maximize/restore and close buttons;
      * maximized client area equals the monitor work area (taskbar visible).

    The main window exposes a diagnostic message (WM_APP + 0x53) returning only
    integers, so this test works across processes without pointer marshaling:
      wParam 0    -> flags: bit0 active, bit1 maximized, bit2 menu mode
      wParam 1    -> title bar height (pixels)
      wParam 2    -> top-level menu count
      wParam 3+4i -> menu i rect: left, top, right, bottom (4 messages)
      wParam 100  -> icon rect (100..103)
      wParam 104  -> minimize button rect (104..107)
      wParam 108  -> maximize button rect (108..111)
      wParam 112  -> close button rect (112..115)
      wParam 116  -> title text rect (116..119)
      wParam 120  -> hovered menu index (-1 = none)
      wParam 121  -> hovered part (TitleBarHit enum; 3 menu, 4 min, 5 max, 6 close)
      wParam 122  -> pressed part
      wParam 123  -> open menu index (-1 = none)
      wParam 124  -> menu mode (0/1)
      wParam 125  -> active title bar (0/1)
      wParam 126  -> keyboard-highlighted menu index (-1 = none)
      wParam 127..130 -> monitor work area: left, top, right, bottom
      wParam 131..134 -> view settings: showEditor, scrollSync, showNav, stacked split
      wParam 200  -> clear injected hover (test hook)
      wParam 201  -> inject menu hover; lParam = menu index
      wParam 202..206 -> inject min/max/close/caption/icon hover

    The app runs with AWM_SETTINGS_DIR pointing into -OutDir, so the user's real
    settings, session and recent files are untouched. Screenshots are saved to
    -OutDir for inspection.

    This is a manual smoke test, not part of CI. Run it from PowerShell:
        pwsh -File tests/manual/titlebar-smoke.ps1
        powershell -ExecutionPolicy Bypass -File tests/manual/titlebar-smoke.ps1

.PARAMETER Exe
    Path to ActiveXWebMark.exe. Defaults to build\vs2022\src\app\Release.

.PARAMETER OutDir
    Artifact folder. Defaults to %TEMP%\awm-titlebar-smoke.

.PARAMETER TimeoutSeconds
    Maximum time to wait for the app to become responsive.
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
# 2. 输出目录与隔离设置
# ---------------------------------------------------------------------------
if (-not $OutDir) {
    $OutDir = Join-Path $env:TEMP "awm-titlebar-smoke"
}
$OutDir = [System.IO.Path]::GetFullPath($OutDir)
if (Test-Path -LiteralPath $OutDir) {
    Remove-Item -LiteralPath $OutDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$document = Join-Path $OutDir "TitleBarSmoke.md"
@"
# 标题栏冒烟测试

用于验证自绘标题栏、菜单和窗口按钮。

- 文件菜单
- 视图菜单
- 最大化 / 最小化 / 关闭
"@ | Set-Content -LiteralPath $document -Encoding utf8

$previousSettingsDir = $env:AWM_SETTINGS_DIR
$env:AWM_SETTINGS_DIR = Join-Path $OutDir "settings"
$previousUpdateCheck = $env:AWM_DISABLE_UPDATE_CHECK
$env:AWM_DISABLE_UPDATE_CHECK = "1"

# ---------------------------------------------------------------------------
# 3. Win32 互操作助手
# ---------------------------------------------------------------------------
Add-Type -AssemblyName System.Drawing
if (-not ([System.Management.Automation.PSTypeName]'AwmTitleBarSmoke').Type) {
    Add-Type -TypeDefinition @"
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class AwmTitleBarSmoke
{
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc cb, IntPtr lParam);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassNameW(IntPtr hWnd, StringBuilder sb, int n);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowTextW(IntPtr hWnd, StringBuilder sb, int n);
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
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hWnd, ref POINT pt);
    [DllImport("user32.dll")] public static extern bool ScreenToClient(IntPtr hWnd, ref POINT pt);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern int GetSystemMetricsForDpi(int index, uint dpi);
    [DllImport("user32.dll")] public static extern bool IsZoomed(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int cmd);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT pt);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint dx, uint dy,
        uint data, IntPtr extra);
    [DllImport("user32.dll")] public static extern IntPtr GetMenu(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern int GetMenuItemCount(IntPtr hMenu);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetMenuStringW(IntPtr hMenu, uint item, StringBuilder sb, int max,
        uint flags);
    [DllImport("user32.dll")] public static extern uint GetMenuState(IntPtr hMenu, uint item,
        uint flags);
    [DllImport("user32.dll")] public static extern bool GetMenuItemRect(IntPtr hWnd, IntPtr hMenu,
        uint item, out RECT rect);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int L, T, R, B; }

    [StructLayout(LayoutKind.Sequential)]
    public struct POINT { public int X, Y; }

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

    public static bool Responsive(IntPtr hWnd, uint timeoutMs)
    {
        IntPtr result;
        return SendMessageTimeoutW(hWnd, 0x0000, IntPtr.Zero, IntPtr.Zero,
            0x0002 /* SMTO_ABORTIFHUNG */, timeoutMs, out result) != IntPtr.Zero;
    }
}
"@
}

# 诊断消息：MainWindow::kTestTitleBarMessage。
$stateMessage = 0x8053

# ---------------------------------------------------------------------------
# 4. 断言、等待、布局读取与消息助手
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

function Round-Px([int]$logical, [uint32]$dpi) {
    return [int][Math]::Floor(($logical * $dpi / 96.0) + 0.5)
}

function Pack-Point([int]$x, [int]$y) {
    return [IntPtr][int](($y -shl 16) -bor ($x -band 0xffff))
}

function Rect-Center($rect) {
    return @([int](($rect[0] + $rect[2]) / 2), [int](($rect[1] + $rect[3]) / 2))
}

function Rect-Width($rect) {
    return [int]($rect[2] - $rect[0])
}

function Rect-Height($rect) {
    return [int]($rect[3] - $rect[1])
}

function Wait-MainWindow([System.Diagnostics.Process]$proc, [int]$timeoutSeconds) {
    $deadline = [DateTime]::UtcNow.AddSeconds($timeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds 200
        $proc.Refresh()
        if ($proc.HasExited) {
            return [IntPtr]::Zero
        }
        $h = [AwmTitleBarSmoke]::FindClass([uint32]$proc.Id, "ActiveXWebMarkMainWindow")
        if ($h -ne [IntPtr]::Zero -and [AwmTitleBarSmoke]::Responsive($h, 100)) {
            return $h
        }
    }
    return [IntPtr]::Zero
}

function Wait-Popup([System.Diagnostics.Process]$proc, [int]$timeoutSeconds) {
    $deadline = [DateTime]::UtcNow.AddSeconds($timeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds 100
        $menu = [AwmTitleBarSmoke]::FindClass([uint32]$proc.Id, "#32768")
        if ($menu -ne [IntPtr]::Zero) {
            return $menu
        }
    }
    return [IntPtr]::Zero
}

function Close-Popup([System.Diagnostics.Process]$proc) {
    $menu = [AwmTitleBarSmoke]::FindClass([uint32]$proc.Id, "#32768")
    if ($menu -eq [IntPtr]::Zero) {
        return
    }
    # Escape is handled by the menu loop and unwinds TrackPopupMenuEx.
    [void][AwmTitleBarSmoke]::PostMessageW($menu, 0x0100, [IntPtr]0x1B, [IntPtr]::Zero)
    for ($i = 0; $i -lt 30; $i++) {
        Start-Sleep -Milliseconds 100
        if ([AwmTitleBarSmoke]::FindClass([uint32]$proc.Id, "#32768") -eq [IntPtr]::Zero) {
            return
        }
    }
    [void][AwmTitleBarSmoke]::PostMessageW($menu, 0x001F, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 300
}

function Get-TitleState([IntPtr]$main) {
    $get = {
        param([int]$what)
        return [AwmTitleBarSmoke]::SendMessageW(
            $main, $stateMessage, [IntPtr]$what, [IntPtr]::Zero).ToInt64()
    }
    return [pscustomobject]@{
        Flags       = & $get 0
        Height      = & $get 1
        MenuCount   = & $get 2
        HoverMenu   = & $get 120
        HoverPart   = & $get 121
        PressPart   = & $get 122
        OpenMenu    = & $get 123
        MenuMode    = & $get 124
        Active      = & $get 125
        Highlight   = & $get 126
        WorkLeft    = & $get 127
        WorkTop     = & $get 128
        WorkRight   = & $get 129
        WorkBottom  = & $get 130
    }
}

function Get-Rect([IntPtr]$main, [int]$base) {
    $get = {
        param([int]$what)
        return [int][AwmTitleBarSmoke]::SendMessageW(
            $main, $stateMessage, [IntPtr]$what, [IntPtr]::Zero).ToInt64()
    }
    return @((& $get $base), (& $get ($base + 1)), (& $get ($base + 2)), (& $get ($base + 3)))
}

function Get-TitleLayout([IntPtr]$main) {
    $state = Get-TitleState $main
    $menus = @()
    for ($i = 0; $i -lt $state.MenuCount; $i++) {
        $menus += ,(Get-Rect $main (3 + 4 * $i))
    }
    return [pscustomobject]@{
        State   = $state
        Height  = [int]$state.Height
        Menus   = $menus
        Icon    = Get-Rect $main 100
        Min     = Get-Rect $main 104
        Max     = Get-Rect $main 108
        Close   = Get-Rect $main 112
        Title   = Get-Rect $main 116
    }
}

# 诊断消息 200..206 直接注入悬停状态（参见 MainWindow::OnTestTitleBarState）。
# 真实光标在自动化环境里会立刻触发 WM_MOUSELEAVE，无法稳定断言悬停。
function Set-TestHover([IntPtr]$main, [int]$what, [int]$value = 0) {
    [void][AwmTitleBarSmoke]::SendMessageW($main, $stateMessage, [IntPtr]$what, [IntPtr]$value)
    Start-Sleep -Milliseconds 100
}

function Send-Click([IntPtr]$main, [int]$x, [int]$y) {
    $lp = Pack-Point $x $y
    [void][AwmTitleBarSmoke]::SendMessageW($main, 0x0201, [IntPtr]1, $lp)
    [void][AwmTitleBarSmoke]::SendMessageW($main, 0x0202, [IntPtr]::Zero, $lp)
    Start-Sleep -Milliseconds 400
}

function Post-Click([IntPtr]$main, [int]$x, [int]$y) {
    $lp = Pack-Point $x $y
    [void][AwmTitleBarSmoke]::PostMessageW($main, 0x0201, [IntPtr]1, $lp)
    [void][AwmTitleBarSmoke]::PostMessageW($main, 0x0202, [IntPtr]::Zero, $lp)
}

function Hit-Test([IntPtr]$main, [int]$clientX, [int]$clientY) {
    $pt = New-Object AwmTitleBarSmoke+POINT
    $pt.X = $clientX
    $pt.Y = $clientY
    [void][AwmTitleBarSmoke]::ClientToScreen($main, [ref]$pt)
    return [int][AwmTitleBarSmoke]::SendMessageW(
        $main, 0x0084, [IntPtr]::Zero, (Pack-Point $pt.X $pt.Y)).ToInt64()
}

function Hit-TestScreen([IntPtr]$main, [int]$screenX, [int]$screenY) {
    return [int][AwmTitleBarSmoke]::SendMessageW(
        $main, 0x0084, [IntPtr]::Zero, (Pack-Point $screenX $screenY)).ToInt64()
}

function Ensure-Active([IntPtr]$main) {
    [void][AwmTitleBarSmoke]::SetForegroundWindow($main)
    Start-Sleep -Milliseconds 300
    $state = Get-TitleState $main
    if ($state.Active -eq 0) {
        # Synthetic activation keeps keyboard-menu assertions deterministic when
        # the automation host owns the foreground.
        [void][AwmTitleBarSmoke]::SendMessageW($main, 0x0006, [IntPtr]1, [IntPtr]::Zero)
        Start-Sleep -Milliseconds 150
    }
}

function Save-Bitmap([IntPtr]$hwnd, [string]$path, [bool]$clientOnly) {
    $rect = New-Object AwmTitleBarSmoke+RECT
    if ($clientOnly) {
        [void][AwmTitleBarSmoke]::GetClientRect($hwnd, [ref]$rect)
    } else {
        [void][AwmTitleBarSmoke]::GetWindowRect($hwnd, [ref]$rect)
    }
    $width = [Math]::Max(1, $rect.R - $rect.L)
    $height = [Math]::Max(1, $rect.B - $rect.T)
    $bitmap = New-Object System.Drawing.Bitmap $width, $height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $hdc = $graphics.GetHdc()
    [void][AwmTitleBarSmoke]::PrintWindow($hwnd, $hdc, 2)
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
$script:originalCursor = $null

try {
    $cursor = New-Object AwmTitleBarSmoke+POINT
    if ([AwmTitleBarSmoke]::GetCursorPos([ref]$cursor)) {
        $script:originalCursor = @([int]$cursor.X, [int]$cursor.Y)
    }

    Write-Host "启动：$Exe" -ForegroundColor Cyan
    Write-Host "输出：$OutDir" -ForegroundColor Cyan
    $proc = Start-Process -FilePath $Exe -ArgumentList ('"' + $document + '"') -PassThru

    $main = Wait-MainWindow $proc $TimeoutSeconds
    if ($main -eq [IntPtr]::Zero) {
        throw "主窗口未出现或未响应（${TimeoutSeconds} 秒）"
    }
    Start-Sleep -Milliseconds 900
    Ensure-Active $main

    $strip = [AwmTitleBarSmoke]::FindWindowExW($main, [IntPtr]::Zero, "AwmTabStrip", $null)
    if ($strip -eq [IntPtr]::Zero) {
        throw "找不到 AwmTabStrip 子窗口"
    }

    $dpi = [AwmTitleBarSmoke]::GetDpiForWindow($main)
    if ($dpi -le 0) { $dpi = 96 }
    $expectedHeight = Round-Px 36 $dpi
    $expectedButton = Round-Px 46 $dpi
    $expectedIcon = Round-Px 16 $dpi
    $expectedStripHeight = Round-Px 32 $dpi

    $client = New-Object AwmTitleBarSmoke+RECT
    [void][AwmTitleBarSmoke]::GetClientRect($main, [ref]$client)
    $layout = Get-TitleLayout $main
    $state = $layout.State
    Save-Bitmap $main (Join-Path $OutDir "01-startup.png") $false

    # 5.1 结构：原生菜单已移除，标题栏菜单/按钮布局正确。
    Check ([AwmTitleBarSmoke]::GetMenu($main) -eq [IntPtr]::Zero) "原生菜单栏已移除（GetMenu = NULL）"
    Check ($layout.State.MenuCount -eq 4) "标题栏包含 4 个顶级菜单"
    Check ($layout.Height -eq $expectedHeight) "标题栏高度按 DPI 缩放（$expectedHeight px）"
    Check (($state.Flags -band 1) -ne 0) "窗口启动后处于激活状态"
    Check ((Rect-Width $layout.Icon) -eq $expectedIcon) "应用图标尺寸按 DPI 缩放（$expectedIcon px）"
    Check ((Rect-Width $layout.Min) -eq $expectedButton -and
           (Rect-Width $layout.Max) -eq $expectedButton -and
           (Rect-Width $layout.Close) -eq $expectedButton) "三个窗口按钮宽度一致（$expectedButton px）"

    $menusOrdered = $true
    for ($i = 0; $i -lt $layout.Menus.Count; $i++) {
        if ((Rect-Height $layout.Menus[$i]) -ne $layout.Height) { $menusOrdered = $false }
        if ($i -gt 0 -and $layout.Menus[$i][0] -lt $layout.Menus[$i - 1][2]) {
            $menusOrdered = $false
        }
    }
    Check $menusOrdered "菜单标签高度正确且从左到右不重叠"
    Check ($layout.Menus[0][0] -gt $layout.Icon[2]) "菜单位于应用图标右侧"
    Check ($layout.Close[2] -eq $client.R) "关闭按钮紧贴客户区右边缘"
    Check ($layout.Min[0] -lt $layout.Max[0] -and $layout.Max[0] -lt $layout.Close[0]) "按钮顺序为 最小化 → 最大化 → 关闭"
    Check ((Rect-Width $layout.Title) -gt 0) "标题区域在菜单与按钮之间展开"
    Check ($layout.Title[2] -le $layout.Min[0]) "标题区域不与最小化按钮重叠"

    # 标签栏必须正好位于标题栏下方。
    $stripRect = New-Object AwmTitleBarSmoke+RECT
    [void][AwmTitleBarSmoke]::GetWindowRect($strip, [ref]$stripRect)
    $stripTopLeft = New-Object AwmTitleBarSmoke+POINT
    $stripTopLeft.X = $stripRect.L
    $stripTopLeft.Y = $stripRect.T
    [void][AwmTitleBarSmoke]::ScreenToClient($main, [ref]$stripTopLeft)
    $stripBottomRight = New-Object AwmTitleBarSmoke+POINT
    $stripBottomRight.X = $stripRect.R
    $stripBottomRight.Y = $stripRect.B
    [void][AwmTitleBarSmoke]::ScreenToClient($main, [ref]$stripBottomRight)
    Check ($stripTopLeft.Y -eq $layout.Height) "标签栏紧贴标题栏下方（top = $($layout.Height)）"
    Check (($stripBottomRight.Y - $stripTopLeft.Y) -eq $expectedStripHeight) "标签栏高度按 DPI 缩放（$expectedStripHeight px）"
    Check (($stripBottomRight.X - $stripTopLeft.X) -eq $client.R) "标签栏占满客户区宽度"

    $titleBuilder = New-Object System.Text.StringBuilder 512
    [void][AwmTitleBarSmoke]::GetWindowTextW($main, $titleBuilder, 512)
    $titleText = $titleBuilder.ToString()
    Check ($titleText -like "*标题栏冒烟测试*" -and $titleText -like "*- ActiveXWebMark") `
        "窗口标题包含文档标题与应用名"

    # 5.2 WM_NCHITTEST：拖动区、菜单/按钮区与缩放边框。
    $menu0Center = Rect-Center $layout.Menus[0]
    Check ((Hit-Test $main $menu0Center[0] $menu0Center[1]) -eq 1) "菜单标签返回 HTCLIENT"
    $iconCenter = Rect-Center $layout.Icon
    Check ((Hit-Test $main $iconCenter[0] $iconCenter[1]) -eq 1) "应用图标返回 HTCLIENT（可打开系统菜单）"
    $minCenter = Rect-Center $layout.Min
    $maxCenter = Rect-Center $layout.Max
    $closeCenter = Rect-Center $layout.Close
    Check ((Hit-Test $main $minCenter[0] $minCenter[1]) -eq 1) "最小化按钮返回 HTCLIENT"
    $maxHit = Hit-Test $main $maxCenter[0] $maxCenter[1]
    Check ($maxHit -eq 1 -or $maxHit -eq 9) "最大化按钮返回 HTCLIENT 或 HTMAXBUTTON（Windows 11 Snap）"
    Check ((Hit-Test $main $closeCenter[0] $closeCenter[1]) -eq 1) "关闭按钮返回 HTCLIENT"

    $titleCenter = Rect-Center $layout.Title
    Check ((Hit-Test $main $titleCenter[0] $titleCenter[1]) -eq 2) "空白标题区返回 HTCAPTION（可拖动/双击最大化）"
    Check ((Hit-Test $main 20 ($layout.Height + $expectedStripHeight + 40)) -eq 1) "标题栏下方返回 HTCLIENT"

    $winRect = New-Object AwmTitleBarSmoke+RECT
    [void][AwmTitleBarSmoke]::GetWindowRect($main, [ref]$winRect)
    $border = [Math]::Max(4,
        [AwmTitleBarSmoke]::GetSystemMetricsForDpi(32, $dpi) +
        [AwmTitleBarSmoke]::GetSystemMetricsForDpi(92, $dpi))
    $midX = [int](($winRect.L + $winRect.R) / 2)
    $midY = [int](($winRect.T + $winRect.B) / 2)
    Check ((Hit-TestScreen $main ($winRect.L + 2) ($winRect.T + 2)) -eq 13) "左上角返回 HTTOPLEFT"
    Check ((Hit-TestScreen $main ($winRect.R - 3) ($winRect.T + 2)) -eq 14) "右上角返回 HTTOPRIGHT"
    Check ((Hit-TestScreen $main ($winRect.L + 2) ($winRect.B - 3)) -eq 16) "左下角返回 HTBOTTOMLEFT"
    Check ((Hit-TestScreen $main ($winRect.R - 3) ($winRect.B - 3)) -eq 17) "右下角返回 HTBOTTOMRIGHT"
    Check ((Hit-TestScreen $main $midX ($winRect.T + 2)) -eq 12) "上边缘返回 HTTOP"
    Check ((Hit-TestScreen $main $midX ($winRect.B - 3)) -eq 15) "下边缘返回 HTBOTTOM"
    Check ((Hit-TestScreen $main ($winRect.L + 2) $midY) -eq 10) "左边缘返回 HTLEFT"
    Check ((Hit-TestScreen $main ($winRect.R - 3) $midY) -eq 11) "右边缘返回 HTRIGHT"
    Check ($border -gt 0) "缩放边框厚度为 $border px"

    # 5.3 悬停状态与截图。通过诊断消息注入悬停，避免真实光标与
    #     TrackMouseEvent 之间的竞态。
    Set-TestHover $main 201 0
    $state = Get-TitleState $main
    Check ($state.HoverPart -eq 3 -and $state.HoverMenu -eq 0) "悬停第一个菜单标签时高亮该标签"
    Set-TestHover $main 204 0
    $state = Get-TitleState $main
    Check ($state.HoverPart -eq 6 -and $state.HoverMenu -eq -1) "悬停关闭按钮时切换按钮高亮"
    Save-Bitmap $main (Join-Path $OutDir "02-close-hover.png") $false
    Set-TestHover $main 202 0
    Check ((Get-TitleState $main).HoverPart -eq 4) "悬停最小化按钮时切换按钮高亮"
    Set-TestHover $main 200 0
    $state = Get-TitleState $main
    Check ($state.HoverPart -eq 0 -and $state.HoverMenu -eq -1) "清除悬停后标题栏恢复常态"

    # 5.4 鼠标打开「文件」菜单并检查内容。
    $layout = Get-TitleLayout $main
    $menu0Center = Rect-Center $layout.Menus[0]
    Post-Click $main $menu0Center[0] $menu0Center[1]
    $popup = Wait-Popup $proc 5
    Check ($popup -ne [IntPtr]::Zero) "点击菜单标签弹出系统下拉菜单"
    if ($popup -ne [IntPtr]::Zero) {
        Check ((Get-TitleState $main).OpenMenu -eq 0) "打开菜单时标题栏记录 openMenu = 0"
        $hMenu = [AwmTitleBarSmoke]::SendMessageW($popup, 0x01E1, [IntPtr]::Zero, [IntPtr]::Zero)
        $itemCount = [AwmTitleBarSmoke]::GetMenuItemCount($hMenu)
        Check ($itemCount -eq 11) "文件菜单包含 11 个项目（含分隔线）"
        $sb = New-Object System.Text.StringBuilder 256
        [void][AwmTitleBarSmoke]::GetMenuStringW($hMenu, [uint32]0, $sb, 256, 0x0400)
        Check ($sb.ToString() -like "打开*") "文件菜单第一项为「打开」"
        $sb = New-Object System.Text.StringBuilder 256
        [void][AwmTitleBarSmoke]::GetMenuStringW($hMenu, [uint32]10, $sb, 256, 0x0400)
        Check ($sb.ToString() -like "退出*") "文件菜单最后一项为「退出」"
        Save-Bitmap $popup (Join-Path $OutDir "03-file-menu.png") $false
        Close-Popup $proc
        Start-Sleep -Milliseconds 300
        Check ((Get-TitleState $main).OpenMenu -eq -1) "关闭菜单后清除标题栏打开状态"
    }

    # 5.5 视图菜单的勾选状态由 WM_INITMENUPOPUP 刷新。
    $layout = Get-TitleLayout $main
    $viewCenter = Rect-Center $layout.Menus[2]
    Post-Click $main $viewCenter[0] $viewCenter[1]
    $popup = Wait-Popup $proc 5
    Check ($popup -ne [IntPtr]::Zero) "点击「视图」弹出菜单"
    if ($popup -ne [IntPtr]::Zero) {
        $hMenu = [AwmTitleBarSmoke]::SendMessageW($popup, 0x01E1, [IntPtr]::Zero, [IntPtr]::Zero)
        Check (([AwmTitleBarSmoke]::GetMenuItemCount($hMenu)) -eq 8) "视图菜单包含 8 个项目"
        $editorState = [AwmTitleBarSmoke]::GetMenuState($hMenu, [uint32]0, 0x0400)
        Check (([uint32]$editorState -band 0x08) -eq 0) "默认隐藏编辑器时「显示编辑器」未勾选"
        $scrollState = [AwmTitleBarSmoke]::GetMenuState($hMenu, [uint32]7, 0x0400)
        Check (([uint32]$scrollState -band 0x08) -ne 0) "「滚动同步」默认为勾选状态"

        # 点击最后一项（滚动同步）。原生菜单只响应真实鼠标输入，向菜单窗口
        # PostMessage 不会执行命令；这里短暂移动真实光标并在之后恢复。
        $itemRect = New-Object AwmTitleBarSmoke+RECT
        if ([AwmTitleBarSmoke]::GetMenuItemRect($main, $hMenu, [uint32]7, [ref]$itemRect)) {
            $itemX = [int](($itemRect.L + $itemRect.R) / 2)
            $itemY = [int](($itemRect.T + $itemRect.B) / 2)
            [void][AwmTitleBarSmoke]::SetCursorPos($itemX, $itemY)
            Start-Sleep -Milliseconds 250
            [AwmTitleBarSmoke]::mouse_event(0x0002, 0, 0, 0, [IntPtr]::Zero) # LEFTDOWN
            Start-Sleep -Milliseconds 60
            [AwmTitleBarSmoke]::mouse_event(0x0004, 0, 0, 0, [IntPtr]::Zero) # LEFTUP
        } else {
            [void][AwmTitleBarSmoke]::PostMessageW(
                $popup, 0x0100, [IntPtr]0x59, [IntPtr]::Zero) # fallback: mnemonic Y
        }
        for ($i = 0; $i -lt 20; $i++) {
            Start-Sleep -Milliseconds 100
            if ([AwmTitleBarSmoke]::FindClass([uint32]$proc.Id, "#32768") -eq [IntPtr]::Zero) {
                break
            }
        }
        if ($script:originalCursor -ne $null) {
            [void][AwmTitleBarSmoke]::SetCursorPos(
                [int]$script:originalCursor[0], [int]$script:originalCursor[1])
            Start-Sleep -Milliseconds 150
        }
        Check ((Get-TitleState $main).OpenMenu -eq -1) "选择菜单项后关闭菜单并执行命令"
        $scrollValue = [AwmTitleBarSmoke]::SendMessageW(
            $main, $stateMessage, [IntPtr]132, [IntPtr]::Zero).ToInt64()
        $editorValue = [AwmTitleBarSmoke]::SendMessageW(
            $main, $stateMessage, [IntPtr]131, [IntPtr]::Zero).ToInt64()
        Check ($scrollValue -eq 0) "选择「滚动同步」后应用状态已切换"
        Check ($editorValue -eq 0) "选择「滚动同步」没有误触其他菜单项"

        $layout = Get-TitleLayout $main
        $viewCenter = Rect-Center $layout.Menus[2]
        Post-Click $main $viewCenter[0] $viewCenter[1]
        $popup = Wait-Popup $proc 5
        Check ($popup -ne [IntPtr]::Zero) "执行命令后重新打开视图菜单"
        if ($popup -ne [IntPtr]::Zero) {
            $hMenu = [AwmTitleBarSmoke]::SendMessageW(
                $popup, 0x01E1, [IntPtr]::Zero, [IntPtr]::Zero)
            $scrollState = [AwmTitleBarSmoke]::GetMenuState($hMenu, [uint32]7, 0x0400)
            Check (([uint32]$scrollState -band 0x08) -eq 0) "执行菜单命令后「滚动同步」变为未勾选"
            Close-Popup $proc
            Start-Sleep -Milliseconds 300
        }
    }

    # 5.6 Alt+F 打开文件菜单。
    Ensure-Active $main
    [void][AwmTitleBarSmoke]::PostMessageW($main, 0x0104, [IntPtr]0x12, [IntPtr]0x00210001)
    Start-Sleep -Milliseconds 150
    [void][AwmTitleBarSmoke]::PostMessageW($main, 0x0104, [IntPtr]0x46, [IntPtr]0x00210001)
    $popup = Wait-Popup $proc 5
    Check ($popup -ne [IntPtr]::Zero) "Alt+F 打开文件菜单"
    if ($popup -ne [IntPtr]::Zero) {
        Check ((Get-TitleState $main).OpenMenu -eq 0) "Alt+F 打开的是文件菜单"
        Close-Popup $proc
        Start-Sleep -Milliseconds 300
    }

    # 5.7 F10 / 方向键 / 回车键盘导航。
    Ensure-Active $main
    [void][AwmTitleBarSmoke]::PostMessageW($main, 0x0100, [IntPtr]0x79, [IntPtr]1) # VK_F10
    Start-Sleep -Milliseconds 300
    $state = Get-TitleState $main
    Check ($state.MenuMode -eq 1) "F10 进入菜单模式"
    Check ($state.Highlight -eq 0) "F10 默认高亮第一个菜单"
    [void][AwmTitleBarSmoke]::PostMessageW($main, 0x0100, [IntPtr]0x27, [IntPtr]1) # VK_RIGHT
    Start-Sleep -Milliseconds 200
    Check ((Get-TitleState $main).Highlight -eq 1) "右方向键移动到第二个菜单"
    [void][AwmTitleBarSmoke]::PostMessageW($main, 0x0100, [IntPtr]0x25, [IntPtr]1) # VK_LEFT
    Start-Sleep -Milliseconds 200
    Check ((Get-TitleState $main).Highlight -eq 0) "左方向键回到第一个菜单"
    [void][AwmTitleBarSmoke]::PostMessageW($main, 0x0100, [IntPtr]0x28, [IntPtr]1) # VK_DOWN
    $popup = Wait-Popup $proc 5
    Check ($popup -ne [IntPtr]::Zero) "下方向键打开当前高亮菜单"
    if ($popup -ne [IntPtr]::Zero) {
        Check ((Get-TitleState $main).OpenMenu -eq 0) "下方向键打开的是第一个菜单"
        Close-Popup $proc
        Start-Sleep -Milliseconds 300
        Check ((Get-TitleState $main).MenuMode -eq 0) "打开菜单后退出键盘菜单模式"
    }

    # 5.8 点击应用图标打开系统菜单。
    $layout = Get-TitleLayout $main
    $iconCenter = Rect-Center $layout.Icon
    Post-Click $main $iconCenter[0] $iconCenter[1]
    $popup = Wait-Popup $proc 5
    Check ($popup -ne [IntPtr]::Zero) "点击应用图标弹出系统菜单"
    if ($popup -ne [IntPtr]::Zero) {
        $hMenu = [AwmTitleBarSmoke]::SendMessageW($popup, 0x01E1, [IntPtr]::Zero, [IntPtr]::Zero)
        Check (([AwmTitleBarSmoke]::GetMenuItemCount($hMenu)) -gt 0) "系统菜单包含标准项目"
        Save-Bitmap $popup (Join-Path $OutDir "04-system-menu.png") $false
        Close-Popup $proc
        Start-Sleep -Milliseconds 300
    }

    # 5.9 最大化 / 还原：客户区必须等于显示器工作区。
    $layout = Get-TitleLayout $main
    $maxCenter = Rect-Center $layout.Max
    Send-Click $main $maxCenter[0] $maxCenter[1]
    Start-Sleep -Milliseconds 700
    Check ([AwmTitleBarSmoke]::IsZoomed($main)) "点击最大化按钮后窗口最大化"
    $state = Get-TitleState $main
    Check (($state.Flags -band 2) -ne 0) "最大化状态通过诊断消息可见"
    $client = New-Object AwmTitleBarSmoke+RECT
    [void][AwmTitleBarSmoke]::GetClientRect($main, [ref]$client)
    $origin = New-Object AwmTitleBarSmoke+POINT
    $origin.X = 0
    $origin.Y = 0
    [void][AwmTitleBarSmoke]::ClientToScreen($main, [ref]$origin)
    $workWidth = $state.WorkRight - $state.WorkLeft
    $workHeight = $state.WorkBottom - $state.WorkTop
    Check ($origin.X -eq $state.WorkLeft -and $origin.Y -eq $state.WorkTop) "最大化后客户区原点等于工作区原点"
    Check ($client.R -eq $workWidth -and $client.B -eq $workHeight) "最大化后客户区尺寸等于工作区（任务栏可见）"
    Save-Bitmap $main (Join-Path $OutDir "05-maximized.png") $false

    $layout = Get-TitleLayout $main
    $maxCenter = Rect-Center $layout.Max
    Send-Click $main $maxCenter[0] $maxCenter[1]
    Start-Sleep -Milliseconds 700
    Check (-not [AwmTitleBarSmoke]::IsZoomed($main)) "再次点击最大化按钮还原窗口"
    Check (((Get-TitleState $main).Flags -band 2) -eq 0) "还原后最大化标志清除"

    # 5.10 最小化后恢复，标题栏状态仍正常。
    Ensure-Active $main
    $layout = Get-TitleLayout $main
    $minCenter = Rect-Center $layout.Min
    Send-Click $main $minCenter[0] $minCenter[1]
    Start-Sleep -Milliseconds 500
    Check ([AwmTitleBarSmoke]::IsIconic($main)) "点击最小化按钮后窗口最小化"
    [void][AwmTitleBarSmoke]::ShowWindow($main, 9) # SW_RESTORE
    Start-Sleep -Milliseconds 600
    Check (-not [AwmTitleBarSmoke]::IsIconic($main)) "ShowWindow(SW_RESTORE) 恢复窗口"
    Ensure-Active $main
    $state = Get-TitleState $main
    Check ($state.Active -eq 1) "恢复后标题栏重新显示为激活状态"
    Check ((Rect-Width (Get-Rect $main 112)) -eq $expectedButton) "恢复后按钮布局仍然有效"

    # 5.11 关闭按钮（放在最后，会结束进程）。
    $layout = Get-TitleLayout $main
    $closeCenter = Rect-Center $layout.Close
    Post-Click $main $closeCenter[0] $closeCenter[1]
    $exited = $false
    for ($i = 0; $i -lt 40; $i++) {
        Start-Sleep -Milliseconds 150
        $proc.Refresh()
        if ($proc.HasExited) {
            $exited = $true
            break
        }
    }
    Check $exited "点击关闭按钮后程序正常退出"
    if ($exited) {
        $main = [IntPtr]::Zero
    }
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
                [void][AwmTitleBarSmoke]::PostMessageW(
                    $main, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
            }
            for ($i = 0; $i -lt 20; $i++) {
                Start-Sleep -Milliseconds 200
                $proc.Refresh()
                if ($proc.HasExited) { break }
            }
            if (-not $proc.HasExited) {
                Stop-Process -Id $proc.Id -Force
            }
        }
    }

    if ($script:originalCursor -ne $null) {
        [void][AwmTitleBarSmoke]::SetCursorPos(
            [int]$script:originalCursor[0], [int]$script:originalCursor[1])
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
