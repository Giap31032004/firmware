param(
    [int]$CaptureCount = 3,
    [string]$MatchPattern = "+-----------------------------------------------+",
    [string]$LogDir = "",
    [string]$RenodeScript = "",
    [string]$RenodeExe = "",
    [int]$MaxSeconds = 30
)

$ErrorActionPreference = "Stop"

if ($LogDir -eq "") {
    $LogDir = Join-Path $PSScriptRoot "..\logs"
}

if ($RenodeScript -eq "") {
    $RenodeScript = Join-Path $PSScriptRoot "..\RenodeOfMe\stm32.resc"
}

function Resolve-RenodeExe {
    param([string]$Requested)

    $candidates = @()
    $isWindows = ($env:OS -eq "Windows_NT")

    if ($Requested -ne "") {
        $candidates += $Requested
    }

    $candidates += Join-Path $PSScriptRoot "tools\renode\renode_1.16.1\renode.exe"
    $candidates += "renode.exe"
    $candidates += "renode"

    if (-not $isWindows) {
        $candidates += Join-Path $PSScriptRoot "tools\renode\renode_1.16.1\renode"
    }

    foreach ($candidate in $candidates) {
        if ([System.IO.Path]::IsPathRooted($candidate) -or $candidate.Contains("\") -or $candidate.Contains("/")) {
            if (Test-Path $candidate) {
                return (Resolve-Path $candidate).Path
            }
        } else {
            $cmd = Get-Command $candidate -ErrorAction SilentlyContinue
            if ($cmd -ne $null) {
                return $cmd.Source
            }
        }
    }

    throw "Renode executable not found. Pass -RenodeExe or add renode to PATH. On Windows, use renode.exe instead of the bundled Linux ELF."
}

$renodePath = Resolve-RenodeExe $RenodeExe
$renodeScriptPath = (Resolve-Path $RenodeScript).Path
$logDirPath = New-Item -ItemType Directory -Force -Path $LogDir

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$logPath = Join-Path $logDirPath.FullName "renode-capture-$timestamp.log"
$latestPath = Join-Path $logDirPath.FullName "renode-capture-latest.log"
$monitorSourcePath = Join-Path (Split-Path -Parent $renodePath) "myos_monitor.log"
$monitorCopyPath = Join-Path $logDirPath.FullName "renode-monitor-$timestamp.log"
$monitorLatestPath = Join-Path $logDirPath.FullName "renode-monitor-latest.log"

$writer = [System.IO.StreamWriter]::new($logPath, $false, [System.Text.Encoding]::UTF8)
$writer.AutoFlush = $true
$sync = New-Object object

$script:matchedCount = 0
$script:stopRequested = $false

$process = [System.Diagnostics.Process]::new()
$process.StartInfo.FileName = $renodePath
$process.StartInfo.Arguments = '--console --disable-xwt "' + $renodeScriptPath + '"'
$process.StartInfo.WorkingDirectory = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$process.StartInfo.UseShellExecute = $false
$process.StartInfo.RedirectStandardOutput = $true
$process.StartInfo.RedirectStandardError = $true
$process.StartInfo.CreateNoWindow = $true
$process.EnableRaisingEvents = $true

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
        $script:matchedCount++
        if ($CaptureCount -gt 0 -and $script:matchedCount -ge $CaptureCount) {
            $script:stopRequested = $true
            if (-not $process.HasExited) {
                try {
                    $process.Kill()
                } catch {
                }
            }
        }
    }
}

try {
    Write-Host "Renode: $renodePath"
    Write-Host "Script: $renodeScriptPath"
    Write-Host "Log: $logPath"
    if (Test-Path $monitorSourcePath) {
        Write-Host "Renode monitor log source: $monitorSourcePath"
    }
    Write-Host "CaptureCount: $CaptureCount, MatchPattern: $MatchPattern"

    $null = $process.add_OutputDataReceived($lineHandler)
    $null = $process.add_ErrorDataReceived($lineHandler)

    if (-not $process.Start()) {
        throw "Failed to start Renode."
    }

    $process.BeginOutputReadLine()
    $process.BeginErrorReadLine()

    $deadline = (Get-Date).AddSeconds($MaxSeconds)
    while (-not $process.HasExited) {
        if ((Get-Date) -ge $deadline) {
            Write-Host "MaxSeconds reached, stopping Renode."
            $script:stopRequested = $true
            $process.Kill()
            break
        }

        Start-Sleep -Milliseconds 100
    }

    $process.WaitForExit()
} finally {
    try {
        $writer.Flush()
        $writer.Close()
    } catch {
    }
}

Copy-Item -Path $logPath -Destination $latestPath -Force

if (Test-Path $monitorSourcePath) {
    Copy-Item -Path $monitorSourcePath -Destination $monitorCopyPath -Force
    Copy-Item -Path $monitorSourcePath -Destination $monitorLatestPath -Force
    Write-Host "Saved monitor log copy: $monitorCopyPath"
    Write-Host "Latest monitor log copy: $monitorLatestPath"
}

Write-Host "Saved log: $logPath"
Write-Host "Latest log: $latestPath"
Write-Host "Matched '$MatchPattern': $script:matchedCount"

if ($CaptureCount -gt 0 -and $script:matchedCount -lt $CaptureCount) {
    exit 2
}

exit 0
