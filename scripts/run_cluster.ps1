param(
    [string]$BuildConfig = "Debug",
    [string]$ZoneId = "starter_city",
    [int]$WorldLoginPort = 9000,
    [int]$ZoneClientPort = 9100
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $RepoRoot "build"

function Resolve-Executable {
    param([string]$Name)
    $path = Join-Path $BuildDir "server\$BuildConfig\$Name.exe"
    if (-not (Test-Path $path)) {
        throw "Missing executable: $path. Build the project first."
    }
    return $path
}

$WorldLoginExe = Resolve-Executable "tbeq_world_login"
$ZoneServerExe = Resolve-Executable "tbeq_zone_server"

Write-Host "Starting WorldLogin on port $WorldLoginPort ..."
$WorldLoginProc = Start-Process -FilePath $WorldLoginExe `
    -ArgumentList @("--dev-mode", "--port", $WorldLoginPort) `
    -WorkingDirectory $RepoRoot `
    -PassThru `
    -WindowStyle Normal

Start-Sleep -Seconds 1

Write-Host "Starting ZoneServer zone=$ZoneId client-port=$ZoneClientPort ..."
$ZoneServerProc = Start-Process -FilePath $ZoneServerExe `
    -ArgumentList @(
        "--dev-mode",
        "--zone-id", $ZoneId,
        "--world-login", "127.0.0.1",
        "--world-login-port", $WorldLoginPort,
        "--client-port", $ZoneClientPort
    ) `
    -WorkingDirectory $RepoRoot `
    -PassThru `
    -WindowStyle Normal

Write-Host ""
Write-Host "Cluster running:"
Write-Host "  WorldLogin PID: $($WorldLoginProc.Id)  port: $WorldLoginPort"
Write-Host "  ZoneServer PID: $($ZoneServerProc.Id)  zone: $ZoneId  client port: $ZoneClientPort"
Write-Host ""
Write-Host "Press Enter to stop the cluster..."
[void][System.Console]::ReadLine()

if (-not $WorldLoginProc.HasExited) {
    Stop-Process -Id $WorldLoginProc.Id -Force
}
if (-not $ZoneServerProc.HasExited) {
    Stop-Process -Id $ZoneServerProc.Id -Force
}

Write-Host "Cluster stopped."
