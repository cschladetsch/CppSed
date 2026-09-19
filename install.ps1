#!/usr/bin/env pwsh
# ============================================================
#  install.ps1 — build (optional) and install fastsed
#
#  Usage:  ./install.ps1 [-Prefix DIR] [-Rebuild] [-NoBuild]
# ============================================================
[CmdletBinding()]
param(
    [string]$Prefix,
    [switch]$Rebuild,
    [switch]$NoBuild
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
    } else {
        $Prefix = Join-Path $HOME ".local"
    }
}

function Get-BuildDir {
    if ($env:FASTSED_BUILD_DIR) {
        return $env:FASTSED_BUILD_DIR
    }
    $binDir = Join-Path $PSScriptRoot "Bin"
    if ((Test-Path $binDir -PathType Container) -and (Test-Path $binDir)) {
        return $binDir
    }
    return Join-Path $PSScriptRoot ".fastsed-build"
}

$BuildDir = Get-BuildDir

if (-not $NoBuild) {
    $buildArgs = @("--prefix", $Prefix, "Release")
    if ($Rebuild) {
        $buildArgs = @("--rebuild", "--prefix", $Prefix, "Release")
    }
    $env:FASTSED_IPO = "ON"
    & ./b @buildArgs
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
} else {
    Write-Host "[install] prefix: $Prefix"
    cmake --install $BuildDir --prefix $Prefix
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
    Write-Host "[install] installed:"
    Write-Host "[install]   $Prefix/bin/fsed"
    Write-Host "[install]   $Prefix/share/man/man1/fsed.1"
}
