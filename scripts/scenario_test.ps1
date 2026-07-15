<#
.SYNOPSIS
    End-to-end scenario test for SampleOrderSystem.exe (Phase 8).

.DESCRIPTION
    Drives the real, already-built SampleOrderSystem.exe (Debug|x64) through a
    full workflow via piped console input: register a sample with a very
    short production time, place two orders (one approved into production,
    one rejected), close the process, sleep past the production time, then
    reopen the process to verify the wall-clock restart catch-up actually
    fires with real elapsed time -- not a simulated `now`. Finally ships the
    resulting CONFIRMED order and checks monitoring reflects RELEASE.

    Assumes scripts/verify.ps1 (or an equivalent build) has already produced
    SampleOrderSystem\x64\Debug\SampleOrderSystem.exe. Does not build anything
    itself.

    The app's data/ folder (next to the exe) is backed up before running and
    restored afterward, since this script's scenario overwrites it.
#>

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$exeDir = Join-Path $repoRoot "SampleOrderSystem\x64\Debug"
$exe = Join-Path $exeDir "SampleOrderSystem.exe"
$dataDir = Join-Path $exeDir "data"
$backupDir = Join-Path $exeDir "data.scenario_backup"

if (-not (Test-Path $exe)) {
    Write-Host "SCENARIO FAIL: executable not found: $exe (run scripts\verify.ps1's build step first)" -ForegroundColor Red
    exit 1
}

function Invoke-App {
    param([string]$InputText)
    # PowerShell's own pipe-to-native-process plumbing (`Get-Content | & $exe`,
    # or piping a string directly) was found to corrupt scripted multi-line
    # input during Phase 8 development -- lines arrived garbled even with
    # plain ASCII content and no BOM. cmd.exe's `<` file redirection is the
    # well-tested standard mechanism for feeding stdin to a console app, so
    # route through cmd.exe /c instead of a PowerShell pipe.
    $tempFile = [System.IO.Path]::GetTempFileName()
    try {
        # ASCII (not UTF8, which Windows PowerShell 5.1 prefixes with a BOM
        # that would corrupt the very first line's integer parse) -- the
        # scenario's scripted input is plain ASCII.
        Set-Content -Path $tempFile -Value $InputText -Encoding ASCII -NoNewline
        $cmdLine = "$exe < $tempFile"
        return (cmd /c $cmdLine 2>&1 | Out-String)
    } finally {
        Remove-Item -Force $tempFile -ErrorAction SilentlyContinue
    }
}

function Assert-Contains {
    param([string]$Output, [string]$Expected, [string]$StepName)
    if ($Output -notlike "*$Expected*") {
        Write-Host "SCENARIO FAIL at '$StepName': expected output to contain '$Expected'" -ForegroundColor Red
        Write-Host "--- actual output ---"
        Write-Host $Output
        exit 1
    }
}

# --- Protect any real data/ the developer already has locally ---
$hadExistingData = Test-Path $dataDir
if ($hadExistingData) {
    Remove-Item -Recurse -Force $backupDir -ErrorAction SilentlyContinue
    Move-Item $dataDir $backupDir
}

try {
    Write-Host "`n=== Scenario step 1: register sample, place+approve one order, place+reject another ===" -ForegroundColor Cyan
    $step1Input = @"
1
1
FastWafer
0.02
1.0
0
2
1
1
CustA
5
3
1
0
2
1
1
CustB
3
4
2
0
0
"@
    $step1Output = Invoke-App -InputText $step1Input
    Assert-Contains -Output $step1Output -Expected "시료가 등록되었습니다" -StepName "register sample"
    Assert-Contains -Output $step1Output -Expected "주문이 접수되었습니다" -StepName "place order"
    Assert-Contains -Output $step1Output -Expected "PRODUCING 상태로 전환" -StepName "approve into production (stock starts at 0)"
    Assert-Contains -Output $step1Output -Expected "주문이 거절되었습니다" -StepName "reject second order"

    Write-Host "`n=== Scenario step 2: wait past the production time, then reopen the app ===" -ForegroundColor Cyan
    # avgProductionTimeMin=0.02 * actualProductionQty=ceil(5/1.0)=5 => totalProductionTimeMin=0.1min=6s
    Start-Sleep -Seconds 8

    $step2Input = @"
3
1
2
0
4
1
2
1
0
3
1
0
0
"@
    $step2Output = Invoke-App -InputText $step2Input
    Assert-Contains -Output $step2Output -Expected "CONFIRMED:1건" -StepName "monitoring shows order confirmed after real restart wait"
    Assert-Contains -Output $step2Output -Expected "고갈" -StepName "stock depleted (surplus was 0 at yield 1.0)"
    Assert-Contains -Output $step2Output -Expected "주문이 출고 처리되었습니다" -StepName "release the confirmed order"
    Assert-Contains -Output $step2Output -Expected "RELEASE:1건" -StepName "monitoring shows the order released"

    Write-Host "`n=== Scenario step 3: verify final data files on disk ===" -ForegroundColor Cyan
    $orders = @(Get-Content (Join-Path $dataDir "orders.json") | ConvertFrom-Json)
    $released = @($orders | Where-Object { $_.status -eq "RELEASE" })
    $rejected = @($orders | Where-Object { $_.status -eq "REJECTED" })
    if ($released.Count -ne 1 -or $rejected.Count -ne 1) {
        Write-Host "SCENARIO FAIL: expected exactly 1 RELEASE and 1 REJECTED order in orders.json" -ForegroundColor Red
        exit 1
    }

    Write-Host "`n=== SCENARIO RESULT: PASS ===" -ForegroundColor Green
}
finally {
    Remove-Item -Recurse -Force $dataDir -ErrorAction SilentlyContinue
    if ($hadExistingData) {
        Move-Item $backupDir $dataDir -Force
    }
}

exit 0
