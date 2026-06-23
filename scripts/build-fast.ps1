param(
    [ValidateSet("client", "servers", "dev", "tests", "tests-all", "all", "release")]
    [string]$Target = "client",
    [ValidateSet("debug-client", "debug-tests", "relwithdebinfo-client", "release-ci", "vs-debug-client")]
    [string]$Preset = "debug-client",
    [switch]$Configure,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

function Import-VisualStudioDevEnvironment
{
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio/Installer/vswhere.exe"
    $vsPath = $null

    if (Test-Path $vswhere)
    {
        $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    }

    if (-not $vsPath)
    {
        $buildToolsRoot = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio/2022/BuildTools"
        if (Test-Path (Join-Path $buildToolsRoot "VC/Auxiliary/Build/vcvars64.bat"))
        {
            $vsPath = $buildToolsRoot
        }
    }

    if (-not $vsPath)
    {
        Write-Warning "Visual Studio C++ tools not found; ensure cl.exe and ninja are on PATH."
        return $null
    }

    $devCmd = Join-Path $vsPath "Common7/Tools/Launch-VsDevShell.ps1"
    $useVcvars = ($vsPath -match "BuildTools")
    if (-not $useVcvars -and (Test-Path $devCmd))
    {
        Write-Host "Loading Visual Studio dev environment..."
        . $devCmd -Arch amd64 -SkipAutomaticLocation -HostArch amd64 2>$null
        $useVcvars = -not (Get-Command cl.exe -ErrorAction SilentlyContinue)
    }
    else
    {
        $useVcvars = $true
    }

    if ($useVcvars)
    {
        $vcvars = Join-Path $vsPath "VC/Auxiliary/Build/vcvars64.bat"
        if (-not (Test-Path $vcvars))
        {
            Write-Warning "vcvars64.bat not found under $vsPath"
            return $null
        }

        Write-Host "Loading MSVC environment via vcvars64.bat..."
        cmd /c "`"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
            if ($_ -match "^(?<name>[^=]+)=(?<value>.*)$")
            {
                Set-Item -Path "Env:$($Matches.name)" -Value $Matches.value
            }
        }
    }

    $ninjaDir = Join-Path $vsPath "Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja"
    if (Test-Path $ninjaDir)
    {
        $env:PATH = "$ninjaDir;$env:PATH"
    }

    return $vsPath
}

function Resolve-BuildPreset
{
    param([string]$TargetName)

    switch ($TargetName)
    {
        "client" { return "debug-client" }
        "servers" { return "debug-servers" }
        "dev" { return "debug-dev" }
        "tests" { return "debug-tests-unit" }
        "tests-all" { return "debug-tests-all" }
        "all" { return "debug-all" }
        "release" { return "release-ci" }
        default { return "debug-client" }
    }
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
Set-Location $repoRoot

$preferredVcpkg = "C:/vcpkg"
if (Test-Path (Join-Path $preferredVcpkg "vcpkg.exe"))
{
    $env:VCPKG_ROOT = $preferredVcpkg
}
elseif (-not $env:VCPKG_ROOT)
{
    throw "VCPKG_ROOT is not set and C:\vcpkg was not found. Set `$env:VCPKG_ROOT to your vcpkg clone."
}
$savedVcpkgRoot = $env:VCPKG_ROOT
Write-Host "VCPKG_ROOT=$savedVcpkgRoot"

Import-VisualStudioDevEnvironment | Out-Null
$env:VCPKG_ROOT = $savedVcpkgRoot

$configurePreset = switch ($Preset)
{
    "debug-client" { "ninja-debug-client" }
    "debug-tests" { "ninja-debug-tests" }
    "relwithdebinfo-client" { "ninja-relwithdebinfo-client" }
    "release-ci" { "ninja-release-ci" }
    "vs-debug-client" { "vs-debug-client" }
}

if ($Configure -or (
        ($Preset -ne "vs-debug-client") -and -not (Test-Path "build/$configurePreset/build.ninja")
    ) -or (
        ($Preset -eq "vs-debug-client") -and -not (Test-Path "build/$configurePreset/TurnBasedEQ.sln")
    ))
{
    Write-Host "Configuring preset: $configurePreset"
    cmake --preset $configurePreset
    if ($LASTEXITCODE -ne 0)
    {
        exit $LASTEXITCODE
    }
}

$buildPreset = Resolve-BuildPreset -TargetName $Target
if ($Preset -eq "vs-debug-client")
{
    $buildPreset = "vs-debug-client"
}
elseif ($Preset -eq "relwithdebinfo-client" -and $Target -eq "client")
{
    $buildPreset = "relwithdebinfo-client"
}
elseif ($Preset -eq "release-ci")
{
    $buildPreset = "release-ci"
}
elseif ($Preset -eq "debug-tests" -and $Target -eq "client")
{
    $buildPreset = "debug-client"
    $configurePreset = "ninja-debug-tests"
}

if ($Clean)
{
    Write-Host "Clean build preset: $buildPreset"
    cmake --build --preset $buildPreset --clean-first
}
else
{
    Write-Host "Build preset: $buildPreset"
    cmake --build --preset $buildPreset
}

if ($LASTEXITCODE -ne 0)
{
    exit $LASTEXITCODE
}

Write-Host ""
Write-Host "Done."
