#requires -Version 5.1
<#
.SYNOPSIS
    Manual smoke test for the theme settings window layout.

.DESCRIPTION
    Verifies:
      * the settings window is resizable (WS_THICKFRAME / WS_MAXIMIZEBOX);
      * the IE preview host does not overlap the save/apply/cancel buttons;
      * the "edit color" button does not overlap the hint label;
      * resizing the window grows the preview and keeps the button row clear.

    Run from PowerShell:
        pwsh -File tests/manual/theme-layout-smoke.ps1
        powershell -ExecutionPolicy Bypass -File tests/manual/theme-layout-smoke.ps1

.PARAMETER Exe
    Path to ActiveXWebMark.exe. Defaults to build\vs2022\src\app\Release.

.PARAMETER OutDir
    Artifact folder. Defaults to %TEMP%\awm-theme-layout-smoke.

.PARAMETER TimeoutSeconds
    Maximum time to wait for the app or the dialog.
#>
[CmdletBinding()]
param(
    [string]$Exe = "",
    [string]$OutDir = "",
    [int]$TimeoutSeconds = 30
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $Exe) {
    $Exe = Join-Path $repoRoot "build\vs2022\src\app\Release\ActiveXWebMark.exe"
}
if (-not (Test-Path -LiteralPath $Exe)) {
    throw "Executable not found: $Exe"
}
$Exe = (Resolve-Path -LiteralPath $Exe).Path
if ($TimeoutSeconds -lt 5) { $TimeoutSeconds = 5 }

if (-not $OutDir) {
    $OutDir = Join-Path $env:TEMP "awm-theme-layout-smoke"
}
$OutDir = [System.IO.Path]::GetFullPath($OutDir)
if (Test-Path -LiteralPath $OutDir) {
    Remove-Item -LiteralPath $OutDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$settingsDir = Join-Path $OutDir "settings"
New-Item -ItemType Directory -Force -Path $settingsDir | Out-Null
'{"ui.theme":"github-light"}' |
    Set-Content -LiteralPath (Join-Path $settingsDir "settings.json") -Encoding ascii

$document = Join-Path $repoRoot "sample.md"
if (-not (Test-Path -LiteralPath $document)) {
    throw "sample.md not found: $document"
}

$previousSettingsDir = $env:AWM_SETTINGS_DIR
$previousUpdateCheck = $env:AWM_DISABLE_UPDATE_CHECK
$env:AWM_SETTINGS_DIR = $settingsDir
$env:AWM_DISABLE_UPDATE_CHECK = "1"

Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;

public class AwmThemeLayoutSmoke {
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr hwnd, int id);
    [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr hwnd, int x, int y, int w, int h, bool repaint);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr hwnd, int index);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr hwnd, StringBuilder cls, int max);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);

    public delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr parent, EnumWindowsProc callback, IntPtr lParam);

    public static IntPtr FindByPid(uint pid, string className) {
        IntPtr found = IntPtr.Zero;
        EnumWindows(delegate(IntPtr hwnd, IntPtr lParam) {
            uint windowPid;
            GetWindowThreadProcessId(hwnd, out windowPid);
            if (windowPid == pid) {
                var cls = new StringBuilder(128);
                GetClassName(hwnd, cls, cls.Capacity);
                if (cls.ToString() == className) {
                    found = hwnd;
                    return false;
                }
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }

    public static IntPtr FindChildClassPrefix(IntPtr parent, string prefix) {
        IntPtr found = IntPtr.Zero;
        EnumChildWindows(parent, delegate(IntPtr hwnd, IntPtr lParam) {
            var cls = new StringBuilder(128);
            GetClassName(hwnd, cls, cls.Capacity);
            if (cls.ToString().StartsWith(prefix)) {
                found = hwnd;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }

    public static bool Intersects(RECT a, RECT b) {
        return !(a.Right <= b.Left || a.Left >= b.Right ||
                 a.Bottom <= b.Top || a.Top >= b.Bottom);
    }
}
'@

$script:failures = New-Object System.Collections.ArrayList
function Check([bool]$condition, [string]$message) {
    if ($condition) {
        Write-Host "  PASS  $message" -ForegroundColor Green
    } else {
        Write-Host "  FAIL  $message" -ForegroundColor Red
        [void]$script:failures.Add($message)
    }
}

$proc = $null
try {
    $proc = Start-Process -FilePath $Exe -ArgumentList $document -PassThru

    $main = [IntPtr]::Zero
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds 100
        $main = [AwmThemeLayoutSmoke]::FindByPid([uint32]$proc.Id, "ActiveXWebMarkMainWindow")
        if ($main -ne [IntPtr]::Zero) { break }
    }
    if ($main -eq [IntPtr]::Zero) { throw "The application did not create a main window." }

    [void][AwmThemeLayoutSmoke]::PostMessage($main, 0x0111, [IntPtr]1023, [IntPtr]::Zero)
    $dialog = [IntPtr]::Zero
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds 100
        $dialog = [AwmThemeLayoutSmoke]::FindByPid([uint32]$proc.Id, "AwmThemeDialog")
        if ($dialog -ne [IntPtr]::Zero) { break }
    }
    if ($dialog -eq [IntPtr]::Zero) { throw "The theme settings window was not created." }
    Start-Sleep -Milliseconds 800

    $style = [AwmThemeLayoutSmoke]::GetWindowLong($dialog, -16)
    Check (($style -band 0x00040000) -ne 0) "window has a resizable frame"
    Check (($style -band 0x00010000) -ne 0) "window has a maximize box"

    $preview = [AwmThemeLayoutSmoke]::FindChildClassPrefix($dialog, "AtlAxWin")
    Check ($preview -ne [IntPtr]::Zero) "preview host exists"
    if ($preview -ne [IntPtr]::Zero) {
        $save = [AwmThemeLayoutSmoke]::GetDlgItem($dialog, 1)
        $apply = [AwmThemeLayoutSmoke]::GetDlgItem($dialog, 226)
        $cancel = [AwmThemeLayoutSmoke]::GetDlgItem($dialog, 2)
        $editColor = [AwmThemeLayoutSmoke]::GetDlgItem($dialog, 208)
        $hint = [AwmThemeLayoutSmoke]::GetDlgItem($dialog, 222)

        $rp = New-Object AwmThemeLayoutSmoke+RECT
        $rs = New-Object AwmThemeLayoutSmoke+RECT
        $ra = New-Object AwmThemeLayoutSmoke+RECT
        $rc = New-Object AwmThemeLayoutSmoke+RECT
        $re = New-Object AwmThemeLayoutSmoke+RECT
        $rh = New-Object AwmThemeLayoutSmoke+RECT
        $rd = New-Object AwmThemeLayoutSmoke+RECT
        [void][AwmThemeLayoutSmoke]::GetWindowRect($preview, [ref]$rp)
        [void][AwmThemeLayoutSmoke]::GetWindowRect($save, [ref]$rs)
        [void][AwmThemeLayoutSmoke]::GetWindowRect($apply, [ref]$ra)
        [void][AwmThemeLayoutSmoke]::GetWindowRect($cancel, [ref]$rc)
        [void][AwmThemeLayoutSmoke]::GetWindowRect($editColor, [ref]$re)
        [void][AwmThemeLayoutSmoke]::GetWindowRect($hint, [ref]$rh)
        [void][AwmThemeLayoutSmoke]::GetWindowRect($dialog, [ref]$rd)

        Check (-not [AwmThemeLayoutSmoke]::Intersects($rp, $rs)) "preview does not cover save"
        Check (-not [AwmThemeLayoutSmoke]::Intersects($rp, $ra)) "preview does not cover apply"
        Check (-not [AwmThemeLayoutSmoke]::Intersects($rp, $rc)) "preview does not cover cancel"
        Check (-not [AwmThemeLayoutSmoke]::Intersects($re, $rh)) "edit-color button does not overlap the hint"

        $splitter = [AwmThemeLayoutSmoke]::GetDlgItem($dialog, 229)
        Check ($splitter -ne [IntPtr]::Zero -and
               [AwmThemeLayoutSmoke]::IsWindowVisible($splitter)) `
            "splitter exists and is visible"
        if ($splitter -ne [IntPtr]::Zero) {
            $oldPreviewHeight = $rp.Bottom - $rp.Top
            $rsplit = New-Object AwmThemeLayoutSmoke+RECT
            [void][AwmThemeLayoutSmoke]::GetWindowRect($splitter, [ref]$rsplit)
            $cx = [int](($rsplit.Left + $rsplit.Right) / 2)
            $cy = [int](($rsplit.Top + $rsplit.Bottom) / 2)
            [void][AwmThemeLayoutSmoke]::SetCursorPos($cx, $cy)
            [void][AwmThemeLayoutSmoke]::SendMessage(
                $splitter, 0x0201, [IntPtr]1, [IntPtr]::Zero)
            [void][AwmThemeLayoutSmoke]::SetCursorPos($cx, $cy - 90)
            [void][AwmThemeLayoutSmoke]::SendMessage(
                $splitter, 0x0200, [IntPtr]1, [IntPtr]::Zero)
            Start-Sleep -Milliseconds 250

            $rpLive = New-Object AwmThemeLayoutSmoke+RECT
            [void][AwmThemeLayoutSmoke]::GetWindowRect($preview, [ref]$rpLive)
            Check (($rpLive.Bottom - $rpLive.Top) -gt $oldPreviewHeight) `
                "splitter follows the mouse during the drag"

            [void][AwmThemeLayoutSmoke]::SendMessage(
                $splitter, 0x0202, [IntPtr]::Zero, [IntPtr]::Zero)
            Start-Sleep -Milliseconds 300

            $rpDragged = New-Object AwmThemeLayoutSmoke+RECT
            [void][AwmThemeLayoutSmoke]::GetWindowRect($preview, [ref]$rpDragged)
            Check (($rpDragged.Bottom - $rpDragged.Top) -gt $oldPreviewHeight) `
                "splitter drag grew the preview"
            Check (($rpDragged.Bottom - $rpDragged.Top) -ge
                   (($rpLive.Bottom - $rpLive.Top) - 1)) `
                "preview keeps the dragged size after release"
            [void][AwmThemeLayoutSmoke]::GetWindowRect($save, [ref]$rs)
            [void][AwmThemeLayoutSmoke]::GetWindowRect($apply, [ref]$ra)
            [void][AwmThemeLayoutSmoke]::GetWindowRect($cancel, [ref]$rc)
            Check (-not [AwmThemeLayoutSmoke]::Intersects($rpDragged, $rs)) `
                "preview stays clear of save after splitter drag"
            Check (-not [AwmThemeLayoutSmoke]::Intersects($rpDragged, $ra)) `
                "preview stays clear of apply after splitter drag"
            Check (-not [AwmThemeLayoutSmoke]::Intersects($rpDragged, $rc)) `
                "preview stays clear of cancel after splitter drag"
            $rp = $rpDragged
        }

        $oldWidth = $rd.Right - $rd.Left
        $oldHeight = $rd.Bottom - $rd.Top
        $oldPreviewHeight = $rp.Bottom - $rp.Top
        [void][AwmThemeLayoutSmoke]::MoveWindow(
            $dialog, $rd.Left, $rd.Top, $oldWidth + 220, $oldHeight + 140, $true)
        Start-Sleep -Milliseconds 800

        $rd2 = New-Object AwmThemeLayoutSmoke+RECT
        $rp2 = New-Object AwmThemeLayoutSmoke+RECT
        [void][AwmThemeLayoutSmoke]::GetWindowRect($dialog, [ref]$rd2)
        [void][AwmThemeLayoutSmoke]::GetWindowRect($preview, [ref]$rp2)
        [void][AwmThemeLayoutSmoke]::GetWindowRect($save, [ref]$rs)
        [void][AwmThemeLayoutSmoke]::GetWindowRect($apply, [ref]$ra)
        [void][AwmThemeLayoutSmoke]::GetWindowRect($cancel, [ref]$rc)

        Check (($rd2.Right - $rd2.Left) -gt $oldWidth) "window width grew"
        Check (($rd2.Bottom - $rd2.Top) -gt $oldHeight) "window height grew"
        Check (($rp2.Bottom - $rp2.Top) -gt $oldPreviewHeight) "preview grew with the window"
        Check (-not [AwmThemeLayoutSmoke]::Intersects($rp2, $rs)) "save stays clear after resize"
        Check (-not [AwmThemeLayoutSmoke]::Intersects($rp2, $ra)) "apply stays clear after resize"
        Check (-not [AwmThemeLayoutSmoke]::Intersects($rp2, $rc)) "cancel stays clear after resize"
    }

    [void][AwmThemeLayoutSmoke]::PostMessage($dialog, 0x0111, [IntPtr]2, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 600
    Check (-not $proc.HasExited) "application still running after cancel"
} finally {
    if ($proc -ne $null -and (Get-Process -Id $proc.Id -ErrorAction SilentlyContinue)) {
        $hwnd = [AwmThemeLayoutSmoke]::FindByPid([uint32]$proc.Id, "ActiveXWebMarkMainWindow")
        if ($hwnd -ne [IntPtr]::Zero) {
            [void][AwmThemeLayoutSmoke]::PostMessage($hwnd, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
        }
        Start-Sleep -Milliseconds 600
        if (Get-Process -Id $proc.Id -ErrorAction SilentlyContinue) {
            Stop-Process -Id $proc.Id -Force
        }
    }
    $env:AWM_SETTINGS_DIR = $previousSettingsDir
    $env:AWM_DISABLE_UPDATE_CHECK = $previousUpdateCheck
}

Write-Host ""
Write-Host "Artifacts: $OutDir"
if ($script:failures.Count -eq 0) {
    Write-Host "SMOKE PASS" -ForegroundColor Green
    exit 0
}
Write-Host "SMOKE FAIL ($($script:failures.Count))" -ForegroundColor Red
foreach ($failure in $script:failures) {
    Write-Host "  - $failure" -ForegroundColor Red
}
exit 1
