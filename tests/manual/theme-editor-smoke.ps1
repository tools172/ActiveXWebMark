#requires -Version 5.1
<#
.SYNOPSIS
    Manual smoke test for user themes and the theme settings window.

.DESCRIPTION
    Launches the application with an isolated settings directory, a hand-written
    `*.awmtheme.json` selected as `ui.theme`, and verifies:
      * the user theme is loaded and applied to the preview and editor;
      * the theme settings window (command 1023) can be created and cancelled
        without crashing the process.

    This complements tests/core/theme_custom_test.cpp (parsers/importers) and
    tests/manual/theme-smoke.ps1 (built-in theme switching).

    Run from PowerShell:
        pwsh -File tests/manual/theme-editor-smoke.ps1
        powershell -ExecutionPolicy Bypass -File tests/manual/theme-editor-smoke.ps1

.PARAMETER Exe
    Path to ActiveXWebMark.exe. Defaults to build\vs2022\src\app\Release.

.PARAMETER OutDir
    Artifact folder. Defaults to %TEMP%\awm-theme-editor-smoke.

.PARAMETER TimeoutSeconds
    Maximum time to wait for the app, a render, or a dialog.
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
if ($TimeoutSeconds -lt 5) {
    $TimeoutSeconds = 5
}

if (-not $OutDir) {
    $OutDir = Join-Path $env:TEMP "awm-theme-editor-smoke"
}
$OutDir = [System.IO.Path]::GetFullPath($OutDir)
if (Test-Path -LiteralPath $OutDir) {
    Remove-Item -LiteralPath $OutDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$settingsDir = Join-Path $OutDir "settings"
$themesDir = Join-Path $settingsDir "themes"
New-Item -ItemType Directory -Force -Path $themesDir | Out-Null
'{"ui.theme":"user-smoke-dark"}' |
    Set-Content -LiteralPath (Join-Path $settingsDir "settings.json") -Encoding ascii

@'
{
  "schemaVersion": 1,
  "id": "user-smoke-dark",
  "name": "Smoke Dark",
  "author": "smoke",
  "dark": true,
  "markdown": {
    "background": "#101010",
    "text": "#eeeeee",
    "accent": "#3399ff",
    "link": "#66bbff"
  }
}
'@ | Set-Content -LiteralPath (Join-Path $themesDir "user-smoke-dark.awmtheme.json") -Encoding utf8

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

public class AwmThemeEditorSmoke {
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr hwnd, int id);
    [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern IntPtr SetFocus(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr hwnd, StringBuilder cls, int max);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr parent, IntPtr after, string cls, string title);

    public delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);

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
        $main = [AwmThemeEditorSmoke]::FindByPid([uint32]$proc.Id, "ActiveXWebMarkMainWindow")
        if ($main -ne [IntPtr]::Zero) { break }
    }
    if ($main -eq [IntPtr]::Zero) { throw "The application did not create a main window." }

    $documentView = [IntPtr]::Zero
    while ([DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds 100
        $documentView = [AwmThemeEditorSmoke]::FindWindowEx($main, [IntPtr]::Zero, "AwmDocumentView", $null)
        if ($documentView -ne [IntPtr]::Zero) { break }
    }
    if ($documentView -eq [IntPtr]::Zero) { throw "The document view was not created." }

    $dark = 0
    $mode = 0
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        $state = [AwmThemeEditorSmoke]::SendMessage(
            $documentView, 0x8054, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
        $dark = [int](($state -shr 25) -band 1)
        $mode = [int](($state -shr 32) -band 0xFF)
        if ($dark -eq 1 -and $mode -eq 11) { break }
        Start-Sleep -Milliseconds 100
    }
    Check ($dark -eq 1) "user theme applied to the preview"
    Check ($mode -eq 11) "preview documentMode is IE11"

    [void][AwmThemeEditorSmoke]::PostMessage($main, 0x0111, [IntPtr]1023, [IntPtr]::Zero)
    $dialog = [IntPtr]::Zero
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds 100
        $dialog = [AwmThemeEditorSmoke]::FindByPid([uint32]$proc.Id, "AwmThemeDialog")
        if ($dialog -ne [IntPtr]::Zero) { break }
    }
    Check ($dialog -ne [IntPtr]::Zero) "theme settings window created"
    if ($dialog -ne [IntPtr]::Zero) {
        $tabs = [AwmThemeEditorSmoke]::GetDlgItem($dialog, 202)
        $themeList = [AwmThemeEditorSmoke]::GetDlgItem($dialog, 201)
        $colorList = [AwmThemeEditorSmoke]::GetDlgItem($dialog, 207)
        $derivedList = [AwmThemeEditorSmoke]::GetDlgItem($dialog, 227)
        $saveButton = [AwmThemeEditorSmoke]::GetDlgItem($dialog, 1)
        $applyButton = [AwmThemeEditorSmoke]::GetDlgItem($dialog, 226)

        Check (([AwmThemeEditorSmoke]::SendMessage($tabs, 0x1304, [IntPtr]::Zero,
            [IntPtr]::Zero).ToInt64()) -eq 4) "theme settings has four tabs"
        Check ([AwmThemeEditorSmoke]::IsWindowEnabled($colorList)) "color list is enabled (scrollable)"
        Check (([AwmThemeEditorSmoke]::SendMessage($colorList, 0x018B, [IntPtr]::Zero,
            [IntPtr]::Zero).ToInt64()) -ge 30) "color list has all rows"
        Check ([AwmThemeEditorSmoke]::IsWindowEnabled($derivedList)) "derived list is enabled"
        Check (([AwmThemeEditorSmoke]::SendMessage($derivedList, 0x018B, [IntPtr]::Zero,
            [IntPtr]::Zero).ToInt64()) -ge 30) "derived list has all rows"

        # Home selects the first built-in theme; End selects the user theme.
        [void][AwmThemeEditorSmoke]::SetForegroundWindow($dialog)
        [void][AwmThemeEditorSmoke]::SetFocus($themeList)
        [void][AwmThemeEditorSmoke]::PostMessage($themeList, 0x0100, [IntPtr]0x24, [IntPtr]::Zero)
        [void][AwmThemeEditorSmoke]::PostMessage($themeList, 0x0101, [IntPtr]0x24, [IntPtr]::Zero)
        Start-Sleep -Milliseconds 400
        Check (-not [AwmThemeEditorSmoke]::IsWindowEnabled($saveButton)) "save is disabled for built-in themes"
        Check ([AwmThemeEditorSmoke]::IsWindowEnabled($applyButton)) "apply is enabled for built-in themes"
        Check ([AwmThemeEditorSmoke]::IsWindowEnabled($colorList)) "built-in color list stays scrollable"

        [void][AwmThemeEditorSmoke]::PostMessage($themeList, 0x0100, [IntPtr]0x23, [IntPtr]::Zero)
        [void][AwmThemeEditorSmoke]::PostMessage($themeList, 0x0101, [IntPtr]0x23, [IntPtr]::Zero)
        Start-Sleep -Milliseconds 400
        Check ([AwmThemeEditorSmoke]::IsWindowEnabled($saveButton)) "save is enabled for user themes"

        [void][AwmThemeEditorSmoke]::PostMessage($dialog, 0x0111, [IntPtr]2, [IntPtr]::Zero)
        Start-Sleep -Milliseconds 600
        $stillOpen = [AwmThemeEditorSmoke]::FindByPid([uint32]$proc.Id, "AwmThemeDialog")
        Check ($stillOpen -eq [IntPtr]::Zero) "theme settings window closed on cancel"
    }
    Check (-not $proc.HasExited) "application still running after closing the dialog"
} finally {
    if ($proc -ne $null -and (Get-Process -Id $proc.Id -ErrorAction SilentlyContinue)) {
        $hwnd = [AwmThemeEditorSmoke]::FindByPid([uint32]$proc.Id, "ActiveXWebMarkMainWindow")
        if ($hwnd -ne [IntPtr]::Zero) {
            [void][AwmThemeEditorSmoke]::PostMessage($hwnd, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
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
