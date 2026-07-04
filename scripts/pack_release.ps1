# Pack KengaEngine demo release: exe + assets + shaders + DLLs + README -> release/ and zip.
# Run from repo root: .\scripts\pack_release.ps1
# Requires: prior Release build (cmake --build build --config Release)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if (-not (Test-Path $root)) { $root = (Get-Location).Path }
$buildRelease = Join-Path $root "build\Release"
$releaseDir = Join-Path $root "release"
$zipName = "KengaEngine-Demo.zip"
$zipPath = Join-Path $root $zipName

if (-not (Test-Path (Join-Path $buildRelease "KengaEngine.exe"))) {
    Write-Error "Release build not found. Run: cmake --build build --config Release"
}

Remove-Item -Path $releaseDir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $releaseDir -Force | Out-Null

Copy-Item (Join-Path $buildRelease "KengaEngine.exe") $releaseDir
Copy-Item (Join-Path $buildRelease "*.dll") $releaseDir
Copy-Item (Join-Path $buildRelease "assets") (Join-Path $releaseDir "assets") -Recurse -Force
Copy-Item (Join-Path $buildRelease "shaders") (Join-Path $releaseDir "shaders") -Recurse -Force
$configDir = Join-Path $root "config"
if (Test-Path $configDir) { Copy-Item $configDir (Join-Path $releaseDir "config") -Recurse -Force }
Copy-Item (Join-Path $root "README.md") $releaseDir
$pkgReadme = Join-Path $root "scripts\README_PACKAGE.txt"
if (Test-Path $pkgReadme) { Copy-Item $pkgReadme $releaseDir }

if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
Compress-Archive -Path (Join-Path $releaseDir "*") -DestinationPath $zipPath

Write-Host "Release packed: $releaseDir"
Write-Host "Zip: $zipPath"
