param(
    [int]$CaptureCount = 3,
    [string]$MatchPattern = "+-----------------------------------------------+",
    [string]$RenodeExe = "",
    [int]$MaxSeconds = 30
)

$captureScript = Join-Path $PSScriptRoot "capture-renode-log.ps1"

$argsList = @(
    "-NoProfile",
    "-ExecutionPolicy", "Bypass",
    "-File", $captureScript,
    "-CaptureCount", $CaptureCount,
    "-MatchPattern", $MatchPattern,
    "-MaxSeconds", $MaxSeconds
)

if ($RenodeExe -ne "") {
    $argsList += @("-RenodeExe", $RenodeExe)
}

& powershell @argsList

exit $LASTEXITCODE
