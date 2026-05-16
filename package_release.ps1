param(
    [switch]$Force  # repackage even if commit hasn't changed
)

$ErrorActionPreference = "Stop"

$root         = $PSScriptRoot
$buildDir     = Join-Path $root "cmake-build-release"
$distDir      = Join-Path $root "dist"
$releasesFile = Join-Path $root "releases.json"
$staging      = Join-Path $root "_staging"
$appName      = "under_attack_orchestrator"

# --- Git info ---
$commitShort = git -C $root rev-parse --short HEAD
$commitMsg   = git -C $root log -1 --pretty=%s

# --- Load releases.json ---
$data     = Get-Content $releasesFile | ConvertFrom-Json
$releases = @($data.releases)
$last     = if ($releases.Count -gt 0) { $releases[-1] } else { $null }

# --- Already packaged? ---
if (-not $Force -and $last -and $last.commit -eq $commitShort) {
    Write-Host "Already packaged as $($last.zip). Nothing to do."
    Write-Host "Use -Force to repackage the same commit."
    exit 0
}

# --- Next version ---
$versionNum = if ($releases.Count -eq 0) { 0 } else { [int]$last.version + 1 }
$versionTag = "v{0:D2}" -f $versionNum
$zipName    = "bajo-ataque-orchestrator-$versionTag.zip"
$zipPath    = Join-Path $distDir $zipName

Write-Host ""
Write-Host "=== Packaging $versionTag ==="
Write-Host "  Commit : $commitShort — $commitMsg"
Write-Host "  Output : $zipName"
Write-Host ""

# --- Initialize MSVC environment ---
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $vcvars = "$vsPath\VC\Auxiliary\Build\vcvars64.bat"
    if (Test-Path $vcvars) {
        Write-Host ">> Initializing MSVC environment..."
        $envLines = cmd /c "`"$vcvars`" > nul 2>&1 && set"
        foreach ($line in $envLines) {
            if ($line -match "^([^=]+)=(.*)$") {
                [System.Environment]::SetEnvironmentVariable($Matches[1], $Matches[2])
            }
        }
    }
}

# --- Build ---
# Reuse existing cmake configuration (e.g. CLion's Release profile) if present.
# If not, configure from scratch — cmake will pick the default generator.
if (-not (Test-Path "$buildDir\CMakeCache.txt")) {
    Write-Host ">> Configuring (no existing build found)..."
    cmake -S $root -B $buildDir -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) { Write-Error "CMake configure failed."; exit 1 }
} else {
    Write-Host ">> Reusing existing cmake configuration in cmake-build-release..."
}

Write-Host ">> Building Release..."
cmake --build $buildDir --config Release
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed."; exit 1 }

# --- Locate build output ---
# Ninja (CLion default): exe sits directly in $buildDir
# VS generator: exe sits in $buildDir\Release\
if (Test-Path "$buildDir\under_attack_orchestrator.exe") {
    $out = $buildDir
} elseif (Test-Path "$buildDir\Release\under_attack_orchestrator.exe") {
    $out = "$buildDir\Release"
} else {
    Write-Error "under_attack_orchestrator.exe not found after build. Expected in $buildDir or $buildDir\Release."
    exit 1
}

# --- Stage files ---
if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
New-Item -ItemType Directory "$staging\plugins\platforms" | Out-Null

foreach ($folder in @("config", "apps", "media", "sounds", "lights", "logs", "tools")) {
    New-Item -ItemType Directory "$staging\$folder" | Out-Null
}

Copy-Item "$out\under_attack_orchestrator.exe" $staging

# Qt DLLs (all that were deployed by CMakeLists.txt post-build)
foreach ($dll in (Get-ChildItem "$out\Qt6*.dll")) {
    Copy-Item $dll.FullName $staging
}

# FFmpeg DLLs required by the multimedia backend
foreach ($dll in (Get-ChildItem "$out\av*.dll", "$out\sw*.dll" -ErrorAction SilentlyContinue)) {
    Copy-Item $dll.FullName $staging
}

# Platform plugin
Copy-Item "$out\plugins\platforms\qwindows.dll" "$staging\plugins\platforms\"

# Multimedia backend plugin
if (Test-Path "$out\plugins\multimedia") {
    New-Item -ItemType Directory "$staging\plugins\multimedia" | Out-Null
    Copy-Item "$out\plugins\multimedia\*" "$staging\plugins\multimedia\" -Recurse
}

$entryDate = Get-Date -Format "yyyy-MM-ddTHH:mm:ss"
$metadata = [PSCustomObject]@{
    app     = $appName
    version = $versionNum
    commit  = $commitShort
    date    = $entryDate
    message = $commitMsg
    zip     = $zipName
}
$metadata | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $staging "version.json") -Encoding UTF8
@(
    "app=$($metadata.app)"
    "version=$($metadata.version)"
    "commit=$($metadata.commit)"
    "date=$($metadata.date)"
    "message=$($metadata.message)"
    "zip=$($metadata.zip)"
) | Set-Content (Join-Path $staging "BUILD_INFO.txt") -Encoding UTF8

# --- Zip ---
Write-Host ">> Creating zip..."
if (-not (Test-Path $distDir)) { New-Item -ItemType Directory $distDir | Out-Null }
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
Compress-Archive -Path "$staging\*" -DestinationPath $zipPath
Remove-Item $staging -Recurse -Force

# --- Update releases.json ---
$entry = [PSCustomObject]@{
    version = $versionNum
    commit  = $commitShort
    date    = $entryDate
    message = $commitMsg
    zip     = $zipName
}
$releases += $entry
$data.releases = $releases
$data | ConvertTo-Json -Depth 5 | Set-Content $releasesFile -Encoding UTF8

# --- Git tag ---
Write-Host ">> Tagging commit as $versionTag..."
git -C $root tag $versionTag 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Host "   Note: tag $versionTag already exists, skipped."
}

# --- Summary ---
$sizeMB = [math]::Round((Get-Item $zipPath).Length / 1MB, 2)
Write-Host ""
Write-Host "=== Done ==="
Write-Host "  Version : $versionTag"
Write-Host "  Commit  : $commitShort — $commitMsg"
Write-Host "  Zip     : $zipName ($sizeMB MB)"
Write-Host "  Path    : $zipPath"
Write-Host ""
