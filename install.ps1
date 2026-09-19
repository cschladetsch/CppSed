#!/usr/bin/env pwsh
# ============================================================
#  install.ps1 — configure, build (optional) and install fastsed
#
#  Usage:  ./install.ps1 [-Prefix DIR] [-Rebuild] [-NoBuild]
#          [-Config Release|Debug]
#
#  Drives cmake directly rather than shelling out to ./b — ./b is a
#  bash script and there's no bash on native Windows PowerShell, so
#  this has to be self-contained to work there.
# ============================================================
[CmdletBinding()]
param(
    [string]$Prefix,
    [switch]$Rebuild,
    [switch]$NoBuild,
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"
Set-Location -Path $PSScriptRoot

if (-not $Prefix) {
    $Prefix = $env:PREFIX
}
if (-not $Prefix) {
    $isRoot = $false
    if ($IsLinux -or $IsMacOS) {
        $isRoot = (id -u) -eq "0"
    }
    if ($isRoot) {
        $Prefix = "/usr/local"
    } elseif ($HOME) {
        $Prefix = Join-Path $HOME ".local"
    } else {
        $Prefix = Join-Path $env:USERPROFILE ".local"
    }
}

function Get-BuildDir {
    if ($env:FASTSED_BUILD_DIR) {
        return $env:FASTSED_BUILD_DIR
    }
    return Join-Path $PSScriptRoot "Bin"
}

$BuildDir = Get-BuildDir

if (-not $NoBuild) {
    if ($Rebuild -and (Test-Path $BuildDir)) {
        Write-Host "[install] removing $BuildDir for a clean reconfigure"
        Remove-Item -Recurse -Force $BuildDir
    }

    Write-Host "[install] configuring ($Config) in $BuildDir/..."
    cmake -B $BuildDir -DCMAKE_BUILD_TYPE=$Config
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    Write-Host "[install] building..."
    cmake --build $BuildDir --config $Config
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host "[install] prefix: $Prefix"
cmake --install $BuildDir --config $Config --prefix $Prefix
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "[install] installed:"
Write-Host "[install]   $Prefix/bin/fsed"
Write-Host "[install]   $Prefix/share/man/man1/fsed.1"
