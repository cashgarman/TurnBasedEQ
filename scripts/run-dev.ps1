param(
    [switch]$Configure,
    [switch]$Clean,
    [switch]$SkipBuild,
    [string[]]$Zones = @("starter_city", "starter_hunting", "starter_dungeon")
)

$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
Set-Location $RepoRoot

$NinjaBuildDir = "build/ninja-debug-client"
$ClientExe = Join-Path $RepoRoot "$NinjaBuildDir/client/tbeq_client.exe"

function Stop-DevCluster
{
    param(
        [System.Diagnostics.Process]$WorldLoginProc,
        [array]$ZoneProcs
    )

    foreach ($entry in ($ZoneProcs | Sort-Object { $_.Port } -Descending))
    {
        if ($null -ne $entry.Proc -and -not $entry.Proc.HasExited)
        {
            Write-Host "Stopping ZoneServer $($entry.ZoneId) (PID $($entry.Proc.Id)) ..."
            Stop-Process -Id $entry.Proc.Id -Force -ErrorAction SilentlyContinue
        }
    }

    Start-Sleep -Milliseconds 500

    if ($null -ne $WorldLoginProc -and -not $WorldLoginProc.HasExited)
    {
        Write-Host "Stopping WorldLogin (PID $($WorldLoginProc.Id)) ..."
        Stop-Process -Id $WorldLoginProc.Id -Force -ErrorAction SilentlyContinue
    }
}

if (-not $SkipBuild)
{
    $buildArgs = @{
        Target = "dev"
        Preset = "debug-client"
    }
    if ($Configure)
    {
        $buildArgs.Configure = $true
    }
    if ($Clean)
    {
        $buildArgs.Clean = $true
    }

    Write-Host "=== Building client + servers (Ninja) ==="
    & (Join-Path $PSScriptRoot "build-fast.ps1") @buildArgs
    if ($LASTEXITCODE -ne 0)
    {
        throw "Build failed."
    }
}

if (-not (Test-Path $ClientExe))
{
    throw "Client executable not found: $ClientExe"
}

Write-Host ""
Write-Host "=== Starting local server cluster ==="
$cluster = & (Join-Path $PSScriptRoot "run_cluster.ps1") `
    -BuildDir $NinjaBuildDir `
    -Zones $Zones `
    -Background

Write-Host ""
Write-Host "=== Launching client ==="
Write-Host $ClientExe
Write-Host ""

$clientExitCode = 0
try
{
    & $ClientExe
    $clientExitCode = $LASTEXITCODE
}
finally
{
    Write-Host ""
    Write-Host "=== Shutting down cluster ==="
    Stop-DevCluster -WorldLoginProc $cluster.WorldLoginProc -ZoneProcs $cluster.ZoneProcs
}

exit $clientExitCode
