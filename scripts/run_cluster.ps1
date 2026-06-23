param(
    [string]$BuildConfig = "Debug",
    [string[]]$Zones = @("starter_city", "starter_hunting", "starter_dungeon"),
    [int]$WorldLoginPort = 9000,
    [int]$WorldLoginClientPort = 9001,
    [int]$ZoneClientPortBase = 9100,
    [string]$DbPath = "config/tbeq.db"
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

function Wait-ForTcpPort {
    param(
        [int]$Port,
        [int]$TimeoutSeconds = 30
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        try {
            $client = New-Object System.Net.Sockets.TcpClient
            $client.Connect("127.0.0.1", $Port)
            $client.Close()
            return $true
        }
        catch {
            Start-Sleep -Milliseconds 100
        }
    }

    return $false
}

function Stop-StaleTbeqProcessesOnPorts {
    param([int[]]$Ports)

    foreach ($port in $Ports) {
        $connections = Get-NetTCPConnection -LocalPort $port -State Listen -ErrorAction SilentlyContinue
        foreach ($connection in $connections) {
            $proc = Get-Process -Id $connection.OwningProcess -ErrorAction SilentlyContinue
            if ($null -eq $proc) {
                continue
            }

            if ($proc.ProcessName -notin @("tbeq_world_login", "tbeq_zone_server")) {
                Write-Warning "Port $port is in use by $($proc.ProcessName) (PID $($proc.Id)); leaving it running."
                continue
            }

            Write-Host "Stopping stale $($proc.ProcessName) (PID $($proc.Id)) on port $port ..."
            Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
        }
    }
}

function Stop-ClusterProcesses {
    param(
        [System.Diagnostics.Process]$WorldLoginProc,
        [array]$ZoneProcs
    )

    foreach ($entry in ($ZoneProcs | Sort-Object { $_.Port } -Descending)) {
        if ($null -ne $entry.Proc -and -not $entry.Proc.HasExited) {
            Write-Host "Stopping ZoneServer $($entry.ZoneId) (PID $($entry.Proc.Id)) ..."
            Stop-Process -Id $entry.Proc.Id -Force -ErrorAction SilentlyContinue
        }
    }

    Start-Sleep -Milliseconds 500

    if ($null -ne $WorldLoginProc -and -not $WorldLoginProc.HasExited) {
        Write-Host "Stopping WorldLogin (PID $($WorldLoginProc.Id)) ..."
        Stop-Process -Id $WorldLoginProc.Id -Force -ErrorAction SilentlyContinue
    }
}

$WorldLoginExe = Resolve-Executable "tbeq_world_login"
$ZoneServerExe = Resolve-Executable "tbeq_zone_server"

$DefaultZonePorts = @{
    "starter_city"    = 9100
    "starter_hunting" = 9101
    "starter_dungeon" = 9102
}

$ClusterPorts = @($WorldLoginPort, $WorldLoginClientPort) + @($DefaultZonePorts.Values)
Write-Host "Cleaning up stale TBEQ server processes on cluster ports ..."
Stop-StaleTbeqProcessesOnPorts -Ports $ClusterPorts
Start-Sleep -Milliseconds 500

Write-Host "Starting WorldLogin on port $WorldLoginPort (client port $WorldLoginClientPort) ..."
$WorldLoginProc = Start-Process -FilePath $WorldLoginExe `
    -ArgumentList @(
        "--dev-mode",
        "--port", $WorldLoginPort,
        "--world-login-client-port", $WorldLoginClientPort,
        "--db-path", (Join-Path $RepoRoot $DbPath),
        "--data-root", (Join-Path $RepoRoot "data")
    ) `
    -WorkingDirectory $RepoRoot `
    -PassThru `
    -WindowStyle Normal

if (-not (Wait-ForTcpPort -Port $WorldLoginClientPort -TimeoutSeconds 30)) {
    if ($WorldLoginProc.HasExited) {
        throw "WorldLogin exited before client port $WorldLoginClientPort became ready (exit code $($WorldLoginProc.ExitCode))."
    }
    throw "WorldLogin client port $WorldLoginClientPort did not become ready within 30 seconds."
}

if ($WorldLoginProc.HasExited) {
    throw "WorldLogin exited during startup (exit code $($WorldLoginProc.ExitCode))."
}

Write-Host "WorldLogin ready on client port $WorldLoginClientPort."

$ZoneProcs = @()
$zoneIndex = 0
foreach ($ZoneId in $Zones) {
    $clientPort = if ($DefaultZonePorts.ContainsKey($ZoneId)) {
        $DefaultZonePorts[$ZoneId]
    } else {
        $ZoneClientPortBase + $zoneIndex
    }
    $zoneIndex++

    Write-Host "Starting ZoneServer zone=$ZoneId client-port=$clientPort ..."
    $proc = Start-Process -FilePath $ZoneServerExe `
        -ArgumentList @(
            "--dev-mode",
            "--zone-id", $ZoneId,
            "--world-login", "127.0.0.1",
            "--world-login-port", $WorldLoginPort,
            "--client-port", $clientPort,
            "--db-path", (Join-Path $RepoRoot $DbPath),
            "--data-root", (Join-Path $RepoRoot "data")
        ) `
        -WorkingDirectory $RepoRoot `
        -PassThru `
        -WindowStyle Normal

    if (-not (Wait-ForTcpPort -Port $clientPort -TimeoutSeconds 30)) {
        if ($proc.HasExited) {
            throw "ZoneServer $ZoneId exited before client port $clientPort became ready (exit code $($proc.ExitCode))."
        }
        throw "ZoneServer $ZoneId client port $clientPort did not become ready within 30 seconds."
    }

    if ($proc.HasExited) {
        throw "ZoneServer $ZoneId exited during startup (exit code $($proc.ExitCode))."
    }

    Write-Host "ZoneServer $ZoneId ready on client port $clientPort."

    $ZoneProcs += [PSCustomObject]@{
        ZoneId = $ZoneId
        Port   = $clientPort
        Proc   = $proc
    }
    Start-Sleep -Milliseconds 500
}

Write-Host ""
Write-Host "Cluster running:"
Write-Host "  WorldLogin PID: $($WorldLoginProc.Id)  port: $WorldLoginPort  client: $WorldLoginClientPort"
foreach ($entry in $ZoneProcs) {
    Write-Host "  ZoneServer PID: $($entry.Proc.Id)  zone: $($entry.ZoneId)  client port: $($entry.Port)"
}
Write-Host ""
Write-Host "Portal navigation: walk to portal tile and press P in the client."
Write-Host "  starter_city    -> north (32,8)  -> Hunting Grounds"
Write-Host "  starter_hunting -> south (64,8)   -> Starter City"
Write-Host "  starter_hunting -> east  (120,64) -> Goblin Cave"
Write-Host "  starter_dungeon -> west  (4,24)   -> Hunting Grounds"
Write-Host ""
Write-Host "Press Enter to stop the cluster..."

try {
    [void][System.Console]::ReadLine()
}
finally {
    Write-Host "Stopping cluster ..."
    Stop-ClusterProcesses -WorldLoginProc $WorldLoginProc -ZoneProcs $ZoneProcs
    Write-Host "Cluster stopped."
}
