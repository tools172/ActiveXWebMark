#requires -Version 5.1
<#
.SYNOPSIS
    Manual smoke test for preview code highlighting and the theme switcher.

.DESCRIPTION
    Launches the built application with a generated Markdown document, switches
    through every built-in theme and verifies, through cross-process window
    messages, that:
      * the preview runs in IE11 document mode (required by PrismJS);
      * Prism loaded and produced token spans for the fenced code blocks;
      * the computed preview background is dark/light as the theme declares;
      * the Scintilla editor background follows the theme;
      * the selected theme id is persisted to settings.json.

    The active DocumentView exposes WM_APP + 0x54, returning a packed preview
    state:
      bits 0-15  token span count
      bits 16-23 language code block count
      bit 24     Prism loaded
      bit 25     body background is dark
      bits 32-39 document.documentMode
      bits 40-47 <script> element count

    Menus are driven with WM_COMMAND (kViewThemeBase + index), so no mouse or
    keyboard focus is required. The app runs with AWM_SETTINGS_DIR pointing into
    -OutDir, leaving the user's real settings untouched.

    Run from PowerShell:
        pwsh -File tests/manual/theme-smoke.ps1
        powershell -ExecutionPolicy Bypass -File tests/manual/theme-smoke.ps1

.PARAMETER Exe
    Path to ActiveXWebMark.exe. Defaults to build\vs2022\src\app\Release.

.PARAMETER OutDir
    Artifact folder. Defaults to %TEMP%\awm-theme-smoke.

.PARAMETER TimeoutSeconds
    Maximum time to wait for the app / a theme render.
#>
[CmdletBinding()]
param(
    [string]$Exe = "",
    [string]$OutDir = "",
    [int]$TimeoutSeconds = 30
)

$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------------------
# 1. Paths and isolated settings
# ---------------------------------------------------------------------------
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
    $OutDir = Join-Path $env:TEMP "awm-theme-smoke"
}
$OutDir = [System.IO.Path]::GetFullPath($OutDir)
if (Test-Path -LiteralPath $OutDir) {
    Remove-Item -LiteralPath $OutDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$settingsDir = Join-Path $OutDir "settings"
New-Item -ItemType Directory -Force -Path $settingsDir | Out-Null
'{"ui.theme":"github-light"}' | Set-Content -LiteralPath (Join-Path $settingsDir "settings.json") -Encoding ascii

$document = Join-Path $OutDir "ThemeSmoke.md"
@'
# Theme smoke test

## C++

```cpp
#include <string>
int main() { const char* s = "hello"; return 42; }
```

## Python

```python
def greet(name):
    return {"name": name, "ok": True}
```

## JavaScript

```js
const answer = 42;
```

## CSS

```css
.demo { color: #123456; }
```

## Bash

```bash
echo "hello"
```
'@ | Set-Content -LiteralPath $document -Encoding utf8

$previousSettingsDir = $env:AWM_SETTINGS_DIR
$previousUpdateCheck = $env:AWM_DISABLE_UPDATE_CHECK
$env:AWM_SETTINGS_DIR = $settingsDir
$env:AWM_DISABLE_UPDATE_CHECK = "1"

# ---------------------------------------------------------------------------
# 2. Win32 helpers
# ---------------------------------------------------------------------------
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;

public class AwmThemeSmoke {
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindowEx(IntPtr parent, IntPtr after, string cls, string title);
    [DllImport("user32.dll")]
    public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr hwnd, uint msg, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")]
    public static extern bool IsWindow(IntPtr hwnd);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassName(IntPtr hwnd, StringBuilder cls, int max);
    public delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr lParam);
    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);
    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int max);
    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hwnd);

    public static IntPtr FindMainWindow(uint pid) {
        IntPtr found = IntPtr.Zero;
        EnumWindows(delegate(IntPtr hwnd, IntPtr lParam) {
            uint windowPid;
            GetWindowThreadProcessId(hwnd, out windowPid);
            if (windowPid == pid) {
                var cls = new StringBuilder(128);
                GetClassName(hwnd, cls, cls.Capacity);
                if (cls.ToString() == "ActiveXWebMarkMainWindow") {
                    found = hwnd;
                    return false;
                }
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }

    public static string DescribeWindows(uint pid) {
        var result = new StringBuilder();
        EnumWindows(delegate(IntPtr hwnd, IntPtr lParam) {
            uint windowPid;
            GetWindowThreadProcessId(hwnd, out windowPid);
            if (windowPid == pid) {
                var cls = new StringBuilder(128);
                var title = new StringBuilder(256);
                GetClassName(hwnd, cls, cls.Capacity);
                GetWindowText(hwnd, title, title.Capacity);
                result.AppendLine("    hwnd=" + hwnd + " visible=" + IsWindowVisible(hwnd) +
                    " class=" + cls + " title=" + title);
            }
            return true;
        }, IntPtr.Zero);
        return result.ToString();
    }
}
'@

function Check([bool]$condition, [string]$message) {
    if ($condition) {
        Write-Host "  PASS  $message" -ForegroundColor Green
    } else {
        Write-Host "  FAIL  $message" -ForegroundColor Red
        [void]$script:failures.Add($message)
    }
}

$script:failures = New-Object System.Collections.ArrayList
$proc = $null

function Get-ThemeState([IntPtr]$documentView) {
    $value = [AwmThemeSmoke]::SendMessage(
        $documentView, 0x8054, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
    return [pscustomobject]@{
        Spans   = [int]($value -band 0xFFFF)
        Blocks  = [int](($value -shr 16) -band 0xFF)
        Prism   = [int](($value -shr 24) -band 1)
        Dark    = [int](($value -shr 25) -band 1)
        Mode    = [int](($value -shr 32) -band 0xFF)
        Scripts = [int](($value -shr 40) -band 0xFF)
    }
}

function Wait-ThemeState([IntPtr]$documentView, [int]$expectedDark, [int]$timeoutMs) {
    $deadline = [DateTime]::UtcNow.AddMilliseconds($timeoutMs)
    while ([DateTime]::UtcNow -lt $deadline) {
        $state = Get-ThemeState $documentView
        if ($state.Prism -eq 1 -and $state.Mode -eq 11 -and $state.Spans -gt 0 -and
            $state.Dark -eq $expectedDark) {
            return $state
        }
        Start-Sleep -Milliseconds 100
    }
    return Get-ThemeState $documentView
}

function Get-EditorBackground([IntPtr]$scintilla) {
    if ($scintilla -eq [IntPtr]::Zero) {
        return $null
    }
    # SCI_STYLEGETBACK = 2482, style 0 is STYLE_DEFAULT.
    $color = [AwmThemeSmoke]::SendMessage(
        $scintilla, 2482, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
    return @(
        [int]($color -band 0xFF),
        [int](($color -shr 8) -band 0xFF),
        [int](($color -shr 16) -band 0xFF)
    )
}

try {
    # -----------------------------------------------------------------------
    # 3. Start the app and locate the document view / editor
    # -----------------------------------------------------------------------
    $proc = Start-Process -FilePath $Exe -ArgumentList $document -PassThru

    $main = [IntPtr]::Zero
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds 100
        $main = [AwmThemeSmoke]::FindMainWindow([uint32]$proc.Id)
        if ($main -ne [IntPtr]::Zero) {
            break
        }
    }
    if ($main -eq [IntPtr]::Zero) {
        throw "The application did not create a main window."
    }

    $documentView = [IntPtr]::Zero
    $scintilla = [IntPtr]::Zero
    while ([DateTime]::UtcNow -lt $deadline) {
        $documentView = [AwmThemeSmoke]::FindWindowEx($main, [IntPtr]::Zero, "AwmDocumentView", $null)
        $scintilla = [AwmThemeSmoke]::FindWindowEx($documentView, [IntPtr]::Zero, "Scintilla", $null)
        if ($documentView -ne [IntPtr]::Zero -and $scintilla -ne [IntPtr]::Zero) {
            break
        }
        Start-Sleep -Milliseconds 100
    }
    if ($documentView -eq [IntPtr]::Zero) {
        throw "The document view was not created."
    }
    Write-Host "  first pid=$($proc.Id) main=$main doc=$documentView"

    $themes = @(
        [pscustomobject]@{ Id = "github-light";     Index = 0; Dark = 0; Bg = @(255, 255, 255) },
        [pscustomobject]@{ Id = "ulysses";          Index = 1; Dark = 0; Bg = @(243, 243, 243) },
        [pscustomobject]@{ Id = "solarized-light";  Index = 2; Dark = 0; Bg = @(253, 246, 227) },
        [pscustomobject]@{ Id = "one-dark";         Index = 3; Dark = 1; Bg = @(40, 44, 52) },
        [pscustomobject]@{ Id = "dracula";          Index = 4; Dark = 1; Bg = @(40, 42, 54) },
        [pscustomobject]@{ Id = "nord";             Index = 5; Dark = 1; Bg = @(46, 52, 64) }
    )

    $settingsPath = Join-Path $settingsDir "settings.json"
    $themeTimeoutMs = [Math]::Max(2000, $TimeoutSeconds * 250)

    # -----------------------------------------------------------------------
    # 4. Switch themes and verify preview, editor and persistence
    # -----------------------------------------------------------------------
    foreach ($theme in $themes) {
        Write-Host "Theme: $($theme.Id)"
        [void][AwmThemeSmoke]::PostMessage(
            $main, 0x0111, [IntPtr](1030 + $theme.Index), [IntPtr]::Zero)

        $state = Wait-ThemeState $documentView $theme.Dark $themeTimeoutMs
        Check ($state.Prism -eq 1) "Prism loaded"
        Check ($state.Mode -eq 11) "preview documentMode is IE11 (was $($state.Mode))"
        Check ($state.Spans -gt 0) "code tokens highlighted (spans=$($state.Spans))"
        Check ($state.Blocks -ge 5) "all fenced blocks found (blocks=$($state.Blocks))"
        Check ($state.Dark -eq $theme.Dark) "preview background dark=$($theme.Dark)"

        $background = Get-EditorBackground $scintilla
        if ($background -eq $null) {
            Check $false "editor background available"
        } else {
            Check ($background[0] -eq $theme.Bg[0] -and
                   $background[1] -eq $theme.Bg[1] -and
                   $background[2] -eq $theme.Bg[2]) `
                "editor background is $($theme.Bg[0]),$($theme.Bg[1]),$($theme.Bg[2]) (got $($background -join ','))"
        }

        $saved = Get-Content -Raw -LiteralPath $settingsPath | ConvertFrom-Json
        Check ($saved.'ui.theme' -eq $theme.Id) "theme persisted as $($theme.Id)"
    }

    # -----------------------------------------------------------------------
    # 5. Restart and confirm the last theme is restored
    # -----------------------------------------------------------------------
    [void][AwmThemeSmoke]::PostMessage($main, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
    for ($i = 0; $i -lt 30; $i++) {
        Start-Sleep -Milliseconds 100
        if (-not (Get-Process -Id $proc.Id -ErrorAction SilentlyContinue)) {
            break
        }
    }
    if (Get-Process -Id $proc.Id -ErrorAction SilentlyContinue) {
        Stop-Process -Id $proc.Id -Force
    }

    Start-Sleep -Milliseconds 800
    $proc = Start-Process -FilePath $Exe -ArgumentList $document -PassThru
    $main = [IntPtr]::Zero
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds 100
        $main = [AwmThemeSmoke]::FindMainWindow([uint32]$proc.Id)
        if ($main -ne [IntPtr]::Zero) {
            break
        }
    }
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $documentView = [IntPtr]::Zero
    while ([DateTime]::UtcNow -lt $deadline) {
        $documentView = [AwmThemeSmoke]::FindWindowEx(
            $main, [IntPtr]::Zero, "AwmDocumentView", $null)
        if ($documentView -ne [IntPtr]::Zero) {
            break
        }
        Start-Sleep -Milliseconds 100
    }
    if ($documentView -eq [IntPtr]::Zero) {
        $ownerPid = [uint32]0
        [void][AwmThemeSmoke]::GetWindowThreadProcessId($main, [ref]$ownerPid)
        Write-Host "  main owner pid=$ownerPid proc=$($proc.Id) exited=$($proc.HasExited)"
        Write-Host ([AwmThemeSmoke]::DescribeWindows([uint32]$proc.Id))
        $child = [AwmThemeSmoke]::FindWindowEx($main, [IntPtr]::Zero, $null, $null)
        $count = 0
        while ($child -ne [IntPtr]::Zero -and $count -lt 20) {
            $cls = New-Object System.Text.StringBuilder 128
            [void][AwmThemeSmoke]::GetClassName($child, $cls, 128)
            Write-Host "  child $child class=$($cls.ToString())"
            $child = [AwmThemeSmoke]::FindWindowEx($main, $child, $null, $null)
            $count++
        }
    }
    Write-Host "  restart pid=$($proc.Id) main=$main doc=$documentView"
    $state = Wait-ThemeState $documentView 1 $themeTimeoutMs
    Write-Host "  restart state: $($state | ConvertTo-Json -Compress)"
    Check ($state.Dark -eq 1) "last dark theme restored after restart"
} finally {
    if ($proc -ne $null -and (Get-Process -Id $proc.Id -ErrorAction SilentlyContinue)) {
        $hwnd = [AwmThemeSmoke]::FindMainWindow([uint32]$proc.Id)
        if ($hwnd -ne [IntPtr]::Zero) {
            [void][AwmThemeSmoke]::PostMessage($hwnd, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
        }
        Start-Sleep -Milliseconds 500
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
