param(
    [int]$Seed = 42,
    [string]$DbPath = "config/tbeq.db",
    [string]$DataRoot = "data",
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$WorldgenExe = Join-Path $RepoRoot "build\tools\Debug\tbeq_worldgen.exe"
if (-not (Test-Path $WorldgenExe)) {
    $WorldgenExe = Join-Path $RepoRoot "build\tools\Release\tbeq_worldgen.exe"
}
if (-not (Test-Path $WorldgenExe)) {
    throw "Missing tbeq_worldgen executable. Build the project first."
}

$args = @(
    "--seed", $Seed,
    "--db-path", (Join-Path $RepoRoot $DbPath),
    "--data-root", (Join-Path $RepoRoot $DataRoot)
)
if ($Force) {
    $args += "--force"
}

Push-Location $RepoRoot
try {
    & $WorldgenExe @args
}
finally {
    Pop-Location
}
