param(
    [ValidateSet("renode", "qemu")]
    [string]$Backend = "renode",
    [string[]]$Tests = @(
        "delay-timeout",
        "sem-timeout",
        "suspend-delay",
        "kill-wait",
        "round-robin",
        "mutex-pi",
        "heap-fragmentation",
        "stack-overflow",
        "queue-timeout",
        "isr-semaphore",
        "binary-semaphore",
        "counting-semaphore",
        "queue-isr",
        "software-timer",
        "api-latency",
        "context-switch",
        "timer-jitter",
        "cpu-load"
    ),
    [int]$MaxSeconds = 30,
    [string]$MakeExe = "",
    [string]$RenodeExe = "",
    [string]$QemuExe = ""
)

$ErrorActionPreference = "Stop"

$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$makeCandidates = @()
if ($MakeExe -ne "") {
    $makeCandidates += $MakeExe
}
$makeCandidates += "make"
$makeCandidates += "mingw32-make"

$makePath = $null
foreach ($candidate in $makeCandidates) {
    $cmd = Get-Command $candidate -ErrorAction SilentlyContinue
    if ($cmd -ne $null) {
        $makePath = $cmd.Source
        break
    }
}

if ($makePath -eq $null) {
    throw "make not found. Install make, add it to PATH, or pass -MakeExe."
}

function Resolve-Tool {
    param(
        [string]$Requested,
        [string[]]$Candidates,
        [string]$Name
    )

    $allCandidates = @()
    if ($Requested -ne "") {
        $allCandidates += $Requested
    }
    $allCandidates += $Candidates

    foreach ($candidate in $allCandidates) {
        $cmd = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($cmd -ne $null) {
            return $cmd.Source
        }
    }

    throw "$Name not found. Add it to PATH or pass the explicit executable path."
}

function Invoke-QemuCapture {
    param(
        [string]$Exe,
        [string]$TestName,
        [string]$MatchPattern,
        [int]$TimeoutSeconds
    )

    $logDir = New-Item -ItemType Directory -Force -Path (Join-Path $root "logs")
    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $logPath = Join-Path $logDir.FullName "qemu-$TestName-$timestamp.log"
    $latestPath = Join-Path $logDir.FullName "qemu-latest.log"
    $elfPath = Join-Path $root "build\myos.elf"

    $writer = [System.IO.StreamWriter]::new($logPath, $false, [System.Text.Encoding]::UTF8)
    $writer.AutoFlush = $true
    $sync = New-Object object
    $script:matched = $false

    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo.FileName = $Exe
    $process.StartInfo.Arguments = "-M netduinoplus2 -kernel `"$elfPath`" -serial mon:stdio -nographic"
    $process.StartInfo.WorkingDirectory = $root
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    $process.StartInfo.CreateNoWindow = $true

    $lineHandler = {
        param($sender, $eventArgs)

        if ($eventArgs.Data -eq $null) {
            return
        }

        [System.Threading.Monitor]::Enter($sync)
        try {
            $writer.WriteLine($eventArgs.Data)
            Write-Host $eventArgs.Data
        } finally {
            [System.Threading.Monitor]::Exit($sync)
        }

        if ($eventArgs.Data -like "*$MatchPattern*") {
            $script:matched = $true
            if (-not $process.HasExited) {
                try {
                    $process.Kill()
                } catch {
                }
            }
        }
    }

    try {
        $null = $process.add_OutputDataReceived($lineHandler)
        $null = $process.add_ErrorDataReceived($lineHandler)

        if (-not $process.Start()) {
            throw "Failed to start QEMU."
        }

        $process.BeginOutputReadLine()
        $process.BeginErrorReadLine()

        $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
        while (-not $process.HasExited) {
            if ((Get-Date) -ge $deadline) {
                $process.Kill()
                break
            }
            Start-Sleep -Milliseconds 100
        }

        $process.WaitForExit()
    } finally {
        $writer.Flush()
        $writer.Close()
    }

    Copy-Item -Path $logPath -Destination $latestPath -Force
    Write-Host "Saved QEMU log: $logPath"

    if (-not $script:matched) {
        throw "QEMU did not emit '$MatchPattern' for $TestName"
    }
}

$scenarios = @{
    "delay-timeout"          = @{ Scenario = 1;  Match = "delay_timeout PASS" }
    "sem-timeout"            = @{ Scenario = 2;  Match = "semaphore_timeout PASS" }
    "suspend-delay"          = @{ Scenario = 3;  Match = "suspend_delay PASS" }
    "kill-wait"              = @{ Scenario = 4;  Match = "kill_wait PASS" }
    "round-robin"            = @{ Scenario = 5;  Match = "round_robin PASS" }
    "mutex-pi"               = @{ Scenario = 6;  Match = "mutex_priority_inheritance PASS" }
    "heap-fragmentation"     = @{ Scenario = 7;  Match = "heap_fragmentation PASS" }
    "stack-overflow"         = @{ Scenario = 8;  Match = "stack overflow" }
    "queue-timeout"          = @{ Scenario = 10; Match = "queue_timeout PASS" }
    "isr-semaphore"          = @{ Scenario = 12; Match = "isr_semaphore PASS" }
    "binary-semaphore"       = @{ Scenario = 13; Match = "binary_semaphore PASS" }
    "counting-semaphore"     = @{ Scenario = 14; Match = "counting_semaphore PASS" }
    "queue-isr"              = @{ Scenario = 15; Match = "queue_from_isr PASS" }
    "software-timer"         = @{ Scenario = 17; Match = "software_timer PASS" }
    "api-latency"            = @{ Scenario = 18; Match = "api_latency PASS" }
    "context-switch"         = @{ Scenario = 19; Match = "context_switch_latency PASS" }
    "timer-jitter"           = @{ Scenario = 20; Match = "timer_jitter PASS" }
    "cpu-load"               = @{ Scenario = 21; Match = "cpu_load PASS" }
}

foreach ($test in $Tests) {
    if (-not $scenarios.ContainsKey($test)) {
        throw "Unknown RTOS test '$test'. Valid tests: $($scenarios.Keys -join ', ')"
    }

    $scenario = $scenarios[$test].Scenario
    $matchPattern = $scenarios[$test].Match
    Write-Host "=== RTOS test: $test (scenario $scenario) ==="

    & $makePath -C $root clean | Write-Host
    if ($LASTEXITCODE -ne 0) {
        throw "make clean failed for $test"
    }

    & $makePath -C $root all "MYOS_TEST_SCENARIO=$scenario" | Write-Host
    if ($LASTEXITCODE -ne 0) {
        throw "make all failed for $test"
    }

    if ($Backend -eq "renode") {
        $args = @(
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-File", (Join-Path $PSScriptRoot "capture-renode-log.ps1"),
            "-CaptureCount", "1",
            "-MatchPattern", $matchPattern,
            "-MaxSeconds", "$MaxSeconds"
        )

        if ($RenodeExe -ne "") {
            $args += @("-RenodeExe", $RenodeExe)
        }

        & powershell @args
        if ($LASTEXITCODE -ne 0) {
            throw "Renode capture failed for $test"
        }
    } else {
        $qemuPath = Resolve-Tool $QemuExe @("qemu-system-arm.exe", "qemu-system-arm") "qemu-system-arm"
        Invoke-QemuCapture $qemuPath $test $matchPattern $MaxSeconds
    }
}
