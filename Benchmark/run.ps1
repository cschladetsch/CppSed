#!/usr/bin/env pwsh
# ============================================================
#  run.ps1 — benchmark fastsed against system sed
#
#  Usage:  ./Benchmark/run.ps1 [-Runs N] [-Warmup N] [-NoBuild]
#          [-CsvOut path] [-SvgOut path] [-PngOut path]
# ============================================================
[CmdletBinding()]
param(
    [int]$Runs = 7,
    [int]$Warmup = 2,
    [switch]$NoBuild,
    [string]$CsvOut,
    [string]$SvgOut,
    [string]$PngOut
)

$ErrorActionPreference = "Stop"
Set-Location -Path (Join-Path $PSScriptRoot "..")

function Get-BuildDir {
    if ($env:FASTSED_BUILD_DIR) {
        return $env:FASTSED_BUILD_DIR
    }
    if (Test-Path "Bin" -PathType Container) {
        return "Bin"
    }
    return ".fastsed-build"
}

$FastsedBin = $env:FASTSED_BIN
if (-not $FastsedBin) {
    $FastsedBin = Join-Path (Get-BuildDir) "fastsed"
}
$SedBin = $env:SED_BIN
if (-not $SedBin) {
    $SedBin = "/usr/bin/sed"
}
$ResultsDir = $env:RESULTS_DIR
if (-not $ResultsDir) {
    $ResultsDir = "Benchmark/Results"
}
if (-not $CsvOut) { $CsvOut = $env:CSV_OUT }
if (-not $CsvOut) { $CsvOut = Join-Path $ResultsDir "latest_results.csv" }
if (-not $SvgOut) { $SvgOut = $env:SVG_OUT }
if (-not $SvgOut) { $SvgOut = Join-Path $ResultsDir "latest_results.svg" }
if (-not $PngOut) { $PngOut = $env:PNG_OUT }
if (-not $PngOut) { $PngOut = Join-Path $ResultsDir "latest_results.png" }

if (-not $NoBuild) {
    & ./b Release
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

if (-not (Test-Path $FastsedBin -PathType Leaf)) {
    Write-Host "[bench] missing fastsed binary: $FastsedBin"
    exit 1
}
if (-not (Test-Path $SedBin -PathType Leaf)) {
    Write-Host "[bench] missing sed binary: $SedBin"
    exit 1
}
if (-not (Get-Command python3 -ErrorAction SilentlyContinue)) {
    Write-Host "[bench] python3 is required"
    exit 1
}

New-Item -ItemType Directory -Force -Path $ResultsDir | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path $CsvOut -Parent) | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path $SvgOut -Parent) | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path $PngOut -Parent) | Out-Null

"name,fastsed_avg,fastsed_best,sed_avg,sed_best,ratio" | Set-Content -Path $CsvOut

$TmpDir = New-Item -ItemType Directory -Path (Join-Path ([System.IO.Path]::GetTempPath()) ([System.Guid]::NewGuid()))
try {
    $LogInput    = Join-Path $TmpDir "log.txt"
    $ConfigInput = Join-Path $TmpDir "config.txt"
    $RepeatInput = Join-Path $TmpDir "repeat.txt"
    $TextInput   = Join-Path $TmpDir "text.txt"
    $CsvInput    = Join-Path $TmpDir "records.csv"
    $PathInput   = Join-Path $TmpDir "paths.txt"
    $NumInput    = Join-Path $TmpDir "numbers.txt"
    $MailInput   = Join-Path $TmpDir "mail.txt"

    function New-LogFixture {
        $lines = for ($i = 1; $i -le 250000; $i++) {
            $level  = if ($i % 19 -eq 0) { "error" } elseif ($i % 11 -eq 0) { "warn" } else { "info" }
            $kind   = if ($i % 7 -eq 0) { "acct" } else { "user" }
            $code   = if ($i % 29 -eq 0) { 500 } elseif ($i % 13 -eq 0) { 404 } else { 200 }
            "{0:D6} level={1} actor={2}-{3} region=ap-southeast-{4} status={5} latency={6}ms payload={7}" -f `
                $i, $level, $kind, ($i * 17), ($i % 3), $code, (($i * 37) % 900), ($i * 97)
        }
        Set-Content -Path $LogInput -Value $lines
    }

    function New-ConfigFixture {
        $lines = for ($i = 1; $i -le 180000; $i++) {
            if ($i % 5 -eq 0) {
                "# comment {0:D6} keep parser busy" -f $i
            } else {
                "key_{0:D6}=value_{1:D6} flag={2} bucket={3}" -f $i, ($i * 3), ($i % 2), ($i % 17)
            }
        }
        Set-Content -Path $ConfigInput -Value $lines
    }

    function New-RepeatFixture {
        $lines = for ($i = 1; $i -le 220000; $i++) {
            "one two two two three two two tail=$i"
        }
        Set-Content -Path $RepeatInput -Value $lines
    }

    function New-TextFixture {
        $lines = for ($i = 1; $i -le 160000; $i++) {
            if ($i % 9 -eq 0) {
                ""
            } else {
                "section_{0:D6}    alpha   beta    gamma   delta_{1}   " -f $i, ($i % 23)
            }
        }
        Set-Content -Path $TextInput -Value $lines
    }

    function New-CsvFixture {
        $lines = for ($i = 1; $i -le 180000; $i++) {
            "acct-{0:D6},team-{1:D2},region-{2},owner-{3:D6}" -f $i, ($i % 40), ($i % 6), ($i * 11)
        }
        Set-Content -Path $CsvInput -Value $lines
    }

    function New-PathsFixture {
        $lines = for ($i = 1; $i -le 180000; $i++) {
            "/srv/app/service_{0:D2}/node_{1:D6}/config.yaml" -f ($i % 25), $i
        }
        Set-Content -Path $PathInput -Value $lines
    }

    function New-NumbersFixture {
        $lines = for ($i = 1; $i -le 250000; $i++) { $i }
        Set-Content -Path $NumInput -Value $lines
    }

    function New-MailFixture {
        $lines = for ($i = 1; $i -le 180000; $i++) {
            "user{0:D6}@example{1}.internal role=team-{2:D2} backup=ops{3:D6}@example{4}.internal" -f `
                $i, ($i % 19), ($i % 31), ($i * 3), ($i % 13)
        }
        Set-Content -Path $MailInput -Value $lines
    }

    New-LogFixture
    New-ConfigFixture
    New-RepeatFixture
    New-TextFixture
    New-CsvFixture
    New-PathsFixture
    New-NumbersFixture
    New-MailFixture

    function Invoke-TimedCommand {
        param([string[]]$CommandArgs)
        & python3 Benchmark/time_command.py @CommandArgs
    }

    function Assert-ValidSample {
        param([string]$Sample)
        if ($Sample -notmatch '^[0-9]+([.][0-9]+)?$') {
            Write-Host "[bench] invalid timing sample: $Sample"
            exit 1
        }
    }

    function Get-SampleSummary {
        param([double[]]$Values)
        $min = ($Values | Measure-Object -Minimum).Minimum
        $avg = ($Values | Measure-Object -Average).Average
        return @($avg, $min)
    }

    function Invoke-BenchmarkCase {
        param(
            [string]$Name,
            [string]$InputFile,
            [string]$Flags,
            [string]$Script
        )

        $fastsedOut = Join-Path $TmpDir "$Name.fastsed.out"
        $sedOut     = Join-Path $TmpDir "$Name.sed.out"
        $flagWords  = @()
        if ($Flags) {
            $flagWords = $Flags -split '\s+' | Where-Object { $_ -ne "" }
        }

        & $FastsedBin @flagWords -f $Script $InputFile | Set-Content -Path $fastsedOut
        & $SedBin @flagWords -f $Script $InputFile | Set-Content -Path $sedOut

        $fastsedContent = Get-Content -Raw -Path $fastsedOut
        $sedContent     = Get-Content -Raw -Path $sedOut
        if ($fastsedContent -ne $sedContent) {
            Write-Host "[bench] output mismatch for $Name"
            & diff -u $sedOut $fastsedOut | Select-Object -First 80
            exit 1
        }

        Write-Host "[bench] $Name"

        for ($i = 0; $i -lt $Warmup; $i++) {
            Invoke-TimedCommand (@($FastsedBin) + $flagWords + @("-f", $Script, $InputFile)) | Out-Null
            Invoke-TimedCommand (@($SedBin) + $flagWords + @("-f", $Script, $InputFile)) | Out-Null
        }

        $fastsedSamples = @()
        $sedSamples = @()
        for ($i = 0; $i -lt $Runs; $i++) {
            $fastsedSample = Invoke-TimedCommand (@($FastsedBin) + $flagWords + @("-f", $Script, $InputFile))
            $sedSample     = Invoke-TimedCommand (@($SedBin) + $flagWords + @("-f", $Script, $InputFile))
            Assert-ValidSample $fastsedSample
            Assert-ValidSample $sedSample
            $fastsedSamples += [double]$fastsedSample
            $sedSamples     += [double]$sedSample
        }

        $fastsedAvg, $fastsedBest = Get-SampleSummary $fastsedSamples
        $sedAvg, $sedBest         = Get-SampleSummary $sedSamples

        $ratio = if ($sedAvg -gt 0.0) { $fastsedAvg / $sedAvg } else { 0.0 }

        "{0},{1:F6},{2:F6},{3:F6},{4:F6},{5:F6}" -f `
            $Name, $fastsedAvg, $fastsedBest, $sedAvg, $sedBest, $ratio | Add-Content -Path $CsvOut

        "{0,-20}  fastsed avg={1,8:F4}s best={2,8:F4}s  sed avg={3,8:F4}s best={4,8:F4}s  ratio={5,6:F3}x" -f `
            $Name, $fastsedAvg, $fastsedBest, $sedAvg, $sedBest, $ratio | Write-Host
    }

    Write-Host "[bench] fastsed: $FastsedBin"
    Write-Host "[bench] sed:     $SedBin"
    Write-Host "[bench] runs=$Runs warmup=$Warmup"
    Write-Host ""

    Invoke-BenchmarkCase "global-substitute"   $LogInput    ""   "Benchmark/Scripts/global-substitute.sed"
    Invoke-BenchmarkCase "extended-capture"    $LogInput    "-E" "Benchmark/Scripts/extended-capture.sed"
    Invoke-BenchmarkCase "address-delete"      $LogInput    ""   "Benchmark/Scripts/address-delete.sed"
    Invoke-BenchmarkCase "delete-comments"     $ConfigInput ""   "Benchmark/Scripts/delete-comments.sed"
    Invoke-BenchmarkCase "transliterate-upper" $LogInput    ""   "Benchmark/Scripts/transliterate-upper.sed"
    Invoke-BenchmarkCase "nth-global"          $RepeatInput ""   "Benchmark/Scripts/nth-global.sed"
    Invoke-BenchmarkCase "compress-spaces"     $TextInput   ""   "Benchmark/Scripts/compress-spaces.sed"
    Invoke-BenchmarkCase "strip-trailing"      $TextInput   ""   "Benchmark/Scripts/strip-trailing.sed"
    Invoke-BenchmarkCase "blank-delete"        $TextInput   ""   "Benchmark/Scripts/blank-delete.sed"
    Invoke-BenchmarkCase "csv-reorder"         $CsvInput    "-E" "Benchmark/Scripts/csv-reorder.sed"
    Invoke-BenchmarkCase "path-rewrite"        $PathInput   ""   "Benchmark/Scripts/path-rewrite.sed"
    Invoke-BenchmarkCase "step-delete"         $NumInput    ""   "Benchmark/Scripts/step-delete.sed"
    Invoke-BenchmarkCase "email-redact"        $MailInput   "-E" "Benchmark/Scripts/email-redact.sed"
    Invoke-BenchmarkCase "error-prefix"        $LogInput    ""   "Benchmark/Scripts/error-prefix.sed"
    Invoke-BenchmarkCase "status-rewrite"      $LogInput    "-E" "Benchmark/Scripts/status-rewrite.sed"

    & python3 Benchmark/render_chart.py $CsvOut $SvgOut
    & convert $SvgOut $PngOut

    Write-Host ""
    Write-Host "[bench] wrote CSV: $CsvOut"
    Write-Host "[bench] wrote SVG: $SvgOut"
    Write-Host "[bench] wrote PNG: $PngOut"
} finally {
    Remove-Item -Recurse -Force -Path $TmpDir -ErrorAction SilentlyContinue
}
