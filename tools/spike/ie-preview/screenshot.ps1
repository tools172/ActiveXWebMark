param(
    [Parameter(Mandatory = $true)][string]$AppPath,
    [Parameter(Mandatory = $true)][string]$MarkdownPath,
    [Parameter(Mandatory = $true)][string]$OutDir,
    [string]$SettingsDir = ''
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class AwmSpikeWin32 {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int cmd);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr after, int x, int y, int cx, int cy, uint flags);
}
"@

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

if ($SettingsDir) {
    # A pre-seeded settings.json selects a theme for the test; otherwise start
    # from an empty directory so session restore cannot affect the screenshot.
    if (-not (Test-Path (Join-Path $SettingsDir 'settings.json'))) {
        Remove-Item -Recurse -Force $SettingsDir -ErrorAction SilentlyContinue
    }
    New-Item -ItemType Directory -Force -Path $SettingsDir | Out-Null
    $env:AWM_SETTINGS_DIR = $SettingsDir
}

$proc = Start-Process -FilePath $AppPath -ArgumentList "`"$MarkdownPath`"" -PassThru
$hwnd = [IntPtr]::Zero
for ($i = 0; $i -lt 80; $i++) {
    Start-Sleep -Milliseconds 100
    $proc.Refresh()
    if ($proc.HasExited) { throw "App exited early (code $($proc.ExitCode))" }
    if ($proc.MainWindowHandle -ne [IntPtr]::Zero) { $hwnd = $proc.MainWindowHandle; break }
}
if ($hwnd -eq [IntPtr]::Zero) { throw "Main window not found" }

# SW_RESTORE + topmost, so the capture cannot pick up another app window.
[void][AwmSpikeWin32]::ShowWindow($hwnd, 9)
[void][AwmSpikeWin32]::SetWindowPos($hwnd, [IntPtr](-1), 40, 40, 1400, 900, 0x0040)
[void][AwmSpikeWin32]::BringWindowToTop($hwnd)
[void][AwmSpikeWin32]::SetForegroundWindow($hwnd)
Start-Sleep -Seconds 5

$rect = [AwmSpikeWin32+RECT]::new()
[void][AwmSpikeWin32]::GetWindowRect($hwnd, [ref]$rect)
$width = $rect.Right - $rect.Left
$height = $rect.Bottom - $rect.Top

$bitmap = New-Object System.Drawing.Bitmap $width, $height
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$hdc = $graphics.GetHdc()
$printed = [AwmSpikeWin32]::PrintWindow($hwnd, $hdc, 2)
$graphics.ReleaseHdc($hdc)
$graphics.Dispose()
$printPath = Join-Path $OutDir 'ie-preview-printwindow.png'
$bitmap.Save($printPath, [System.Drawing.Imaging.ImageFormat]::Png)
$bitmap.Dispose()

$screen = New-Object System.Drawing.Bitmap $width, $height
$screenGraphics = [System.Drawing.Graphics]::FromImage($screen)
$screenGraphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, (New-Object System.Drawing.Size $width, $height))
$screenGraphics.Dispose()
$screenPath = Join-Path $OutDir 'ie-preview-screen.png'
$screen.Save($screenPath, [System.Drawing.Imaging.ImageFormat]::Png)
$screen.Dispose()

# Leave the window normal (not topmost) for the next run.
[void][AwmSpikeWin32]::SetWindowPos($hwnd, [IntPtr](-2), 0, 0, 0, 0, 0x0001 -bor 0x0002)

[pscustomobject]@{
    ProcessId = $proc.Id
    WindowHandle = $hwnd.ToInt64()
    Title = $proc.MainWindowTitle
    Visible = [AwmSpikeWin32]::IsWindowVisible($hwnd)
    Width = $width
    Height = $height
    PrintWindow = $printed
    PrintWindowPath = $printPath
    ScreenPath = $screenPath
} | ConvertTo-Json -Compress
