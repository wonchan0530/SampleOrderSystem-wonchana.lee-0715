<#
.SYNOPSIS
    Harness for SampleOrderSystem: builds all configurations and runs the test suite.

.DESCRIPTION
    Step 4 ("Harness 실행") of the per-phase development process defined in PLAN.md.
    Builds SampleOrderSystem.slnx for (Debug, Release) x (x86, x64), then runs
    SampleOrderSystemTests.exe (Debug|x64 output) and reports its result.
    Exits with a non-zero code if any build or the test run fails.
#>

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$solution = Join-Path $repoRoot "SampleOrderSystem\SampleOrderSystem.slnx"

function Find-MSBuild {
    $vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe not found at '$vswhere'. Is Visual Studio installed?"
    }
    $msbuildPath = & $vswhere -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe |
        Where-Object { $_ -like "*\18\*" } |
        Select-Object -First 1
    if (-not $msbuildPath) {
        throw "MSBuild.exe for Visual Studio 2026 (v18, PlatformToolset v145) not found."
    }
    return $msbuildPath
}

$msbuild = Find-MSBuild
Write-Host "Using MSBuild: $msbuild"

$configs = @(
    @{ Configuration = "Debug";   Platform = "x86" },
    @{ Configuration = "Release"; Platform = "x86" },
    @{ Configuration = "Debug";   Platform = "x64" },
    @{ Configuration = "Release"; Platform = "x64" }
)

$buildFailures = @()

foreach ($config in $configs) {
    $label = "$($config.Configuration)|$($config.Platform)"
    Write-Host "`n=== Building $label ===" -ForegroundColor Cyan

    & $msbuild $solution /nologo /v:minimal `
        "/p:Configuration=$($config.Configuration)" `
        "/p:Platform=$($config.Platform)"

    if ($LASTEXITCODE -ne 0) {
        Write-Host "BUILD FAILED: $label" -ForegroundColor Red
        $buildFailures += $label
    } else {
        Write-Host "Build OK: $label" -ForegroundColor Green
    }
}

if ($buildFailures.Count -gt 0) {
    Write-Host "`n=== HARNESS RESULT: FAIL ===" -ForegroundColor Red
    Write-Host "Failed builds: $($buildFailures -join ', ')"
    exit 1
}

Write-Host "`n=== Running tests (Debug|x64) ===" -ForegroundColor Cyan
$testExe = Join-Path $repoRoot "SampleOrderSystem\x64\Debug\SampleOrderSystemTests.exe"
if (-not (Test-Path $testExe)) {
    Write-Host "HARNESS RESULT: FAIL (test executable not found: $testExe)" -ForegroundColor Red
    exit 1
}

Push-Location (Split-Path -Parent $testExe)
try {
    & $testExe
    $testExitCode = $LASTEXITCODE
} finally {
    Pop-Location
}

if ($testExitCode -ne 0) {
    Write-Host "`n=== HARNESS RESULT: FAIL (tests failed) ===" -ForegroundColor Red
    exit 1
}

Write-Host "`n=== HARNESS RESULT: PASS (4/4 builds, tests green) ===" -ForegroundColor Green
exit 0
