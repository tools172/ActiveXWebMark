# Downloads and verifies the pinned PlantUML headless native build.
#
# Packaging must not continue if this script fails: it exits 1 with a clear
# message, and the installer scripts also guard on the staged executable.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools\plantuml-native\fetch.ps1
#   ... -ZipPath D:\plantuml-headless.zip     # offline / manual download
#   ... -Url https://mirror/...               # mirror
#   ... -VerifyOnly                           # verify the cached zip only
#   ... -Force                                # re-download

param(
    [string]$Url = '',
    [string]$ZipPath = '',
    [switch]$Force,
    [switch]$VerifyOnly
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$manifest = Get-Content -Raw -Path (Join-Path $PSScriptRoot 'manifest.json') | ConvertFrom-Json

$buildRoot = Join-Path $repoRoot 'build'
$buildDir = Join-Path $buildRoot 'plantuml-native'
$cacheDir = Join-Path $buildDir 'cache'
$staging = Join-Path $buildDir 'staging'
$extractDir = Join-Path $buildDir 'extract'
$logPath = Join-Path $buildDir 'fetch.log'
$zip = ''

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
New-Item -ItemType Directory -Force -Path $cacheDir | Out-Null
if (Test-Path $logPath) {
    Remove-Item -Force $logPath
}

function Log([string]$message) {
    Add-Content -Path $logPath -Value ('[{0}] {1}' -f (Get-Date -Format o), $message)
}

function Fail([string]$stage, [string]$message, [string]$actual = '') {
    Log "FAIL [$stage] $message"
    Write-Host ''
    Write-Host 'PlantUML native download FAILED' -ForegroundColor Red
    Write-Host "Stage:        $stage"
    Write-Host "Version:      $($manifest.version)"
    Write-Host "URL:          $Url"
    Write-Host "Expected SHA: $($manifest.sha256)"
    if ($actual) {
        Write-Host "Actual SHA:   $actual"
    }
    Write-Host "Cached zip:   $zip"
    Write-Host "Log:          $logPath"
    Write-Host 'Hint:         可手动下载后运行 tools/plantuml-native/fetch.ps1 -ZipPath <本地zip>'
    if ($message) {
        Write-Host "Detail:       $message"
    }
    exit 1
}

if (-not $Url) {
    $Url = $env:AWM_PLANTUML_NATIVE_URL
    if (-not $Url) {
        $Url = $manifest.url
    }
}

$expected = $manifest.sha256.ToLowerInvariant()

if ($ZipPath) {
    if (-not (Test-Path $ZipPath)) {
        Fail 'local-zip' "file not found: $ZipPath"
    }
    $zip = (Resolve-Path $ZipPath).Path
} else {
    $zip = Join-Path $cacheDir ('plantuml-headless-{0}.zip' -f $manifest.version)
    $needDownload = $Force -or -not (Test-Path $zip)
    if (-not $needDownload) {
        $cachedHash = (Get-FileHash -Algorithm SHA256 -Path $zip).Hash.ToLowerInvariant()
        if ($cachedHash -ne $expected) {
            Log 'cached zip checksum mismatch; re-downloading'
            Remove-Item -Force $zip
            $needDownload = $true
        }
    }

    if ($needDownload) {
        if ($VerifyOnly) {
            Fail 'verify-only' 'cached zip is missing; run without -VerifyOnly to download'
        }
        $part = "$zip.part"
        if (Test-Path $part) {
            Remove-Item -Force $part
        }
        $downloadOk = $false
        for ($attempt = 1; $attempt -le 3 -and -not $downloadOk; $attempt++) {
            Log "download attempt ${attempt}: $Url"
            try {
                $curl = Get-Command curl.exe -ErrorAction SilentlyContinue
                if ($curl) {
                    & $curl.Source -L --fail --retry 2 --connect-timeout 20 --max-time 600 -o $part $Url
                    if ($LASTEXITCODE -ne 0) {
                        throw "curl exit code $LASTEXITCODE"
                    }
                } else {
                    Invoke-WebRequest -Uri $Url -OutFile $part -UseBasicParsing
                }
                $downloadOk = $true
            } catch {
                Log "download attempt $attempt failed: $($_.Exception.Message)"
                if (Test-Path $part) {
                    Remove-Item -Force $part -ErrorAction SilentlyContinue
                }
                Start-Sleep -Seconds 2
            }
        }
        if (-not $downloadOk) {
            Fail 'download' 'could not download the zip (see log)'
        }
        Move-Item -Force $part $zip
    }
}

if (-not (Test-Path $zip)) {
    Fail 'download' "zip not found: $zip"
}

$actual = (Get-FileHash -Algorithm SHA256 -Path $zip).Hash.ToLowerInvariant()
Log "checksum actual=$actual expected=$expected"
if ($actual -ne $expected) {
    Fail 'checksum' 'SHA-256 mismatch' $actual
}

if ($VerifyOnly) {
    Write-Host "PlantUML native zip OK: $zip" -ForegroundColor Green
    Write-Host "SHA256: $actual"
    exit 0
}

if (Test-Path $extractDir) {
    Remove-Item -Recurse -Force $extractDir
}
New-Item -ItemType Directory -Force -Path $extractDir | Out-Null
try {
    Expand-Archive -Path $zip -DestinationPath $extractDir -Force
} catch {
    Fail 'extract' $_.Exception.Message
}

$exe = Get-ChildItem -Path $extractDir -Recurse -Filter $manifest.exe | Select-Object -First 1
if (-not $exe) {
    Fail 'extract' "$($manifest.exe) not found in the archive"
}

if (Test-Path $staging) {
    Remove-Item -Recurse -Force $staging
}
New-Item -ItemType Directory -Force -Path $staging | Out-Null
Copy-Item -Path (Join-Path $exe.Directory.FullName '*') -Destination $staging -Recurse -Force

foreach ($licenseName in @('LICENSE.plantuml', 'license.txt')) {
    $license = Join-Path $PSScriptRoot $licenseName
    if (Test-Path $license) {
        Copy-Item -Path $license -Destination (Join-Path $staging $licenseName) -Force
    }
}

$stagedExe = Join-Path $staging $manifest.exe
if (-not (Test-Path $stagedExe)) {
    Fail 'staging' "staged executable missing: $stagedExe"
}

Log "smoke test: $stagedExe -version"
$smokeOutput = & $stagedExe -version 2>&1
$smokeExit = $LASTEXITCODE
$smokeText = ($smokeOutput | Out-String)
Log "smoke test exit=$smokeExit"

# plantuml -version prints the version and then exits with the Graphviz status
# from GraphvizUtils.addDotStatus(): 0 when dot is usable, 16 when it is not.
# A build agent without Graphviz is expected to report 16, so treat it as a
# warning; the version banner below still proves the native binary runs.
if ($smokeText -notmatch 'PlantUML version') {
    Log "smoke test output: $smokeText"
    Fail 'smoke-test' "plantuml-headless.exe -version printed no version banner (exit $smokeExit)"
}
if ($smokeExit -ne 0 -and $smokeExit -ne 16) {
    Log "smoke test output: $smokeText"
    Fail 'smoke-test' "plantuml-headless.exe -version failed (exit $smokeExit)"
}
if ($smokeExit -eq 16) {
    $graphvizNote = 'Graphviz (dot) not found: -version exited 16 (only sequence diagrams)'
    Log $graphvizNote
    Write-Host "Note:    $graphvizNote" -ForegroundColor Yellow
}
Log "smoke test ok: $($smokeText.Trim())"

Write-Host "PlantUML native ready: $($manifest.exe) $($manifest.version)" -ForegroundColor Green
Write-Host "Staging: $staging"
Write-Host "SHA256:  $actual"
exit 0
