param(
    [string]$PackageRoot = (Join-Path $PSScriptRoot "..\\out")
)

$ErrorActionPreference = "Stop"

$resolved = Resolve-Path $PackageRoot
$required = @(
    "RePlayer.exe",
    "recordings",
    "logs",
    "config"
)

foreach ($item in $required) {
    $target = Join-Path $resolved $item
    if (-not (Test-Path $target)) {
        throw "Missing package artifact: $item"
    }
}

$logProbe = Join-Path $resolved "logs\\write-test.txt"
"ok" | Set-Content $logProbe -Encoding UTF8
if (-not (Test-Path $logProbe)) {
    throw "Logs directory is not writable."
}
Remove-Item $logProbe -Force

$recordingProbe = Join-Path $resolved "recordings\\write-test.txt"
"ok" | Set-Content $recordingProbe -Encoding UTF8
if (-not (Test-Path $recordingProbe)) {
    throw "Recordings directory is not writable."
}
Remove-Item $recordingProbe -Force

Write-Host "Package verification passed: $resolved"
