param(
    [string]$CacheDir = (Join-Path $PSScriptRoot ".." "third_party" "cache")
)

$ErrorActionPreference = "Stop"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

New-Item -ItemType Directory -Force -Path $CacheDir | Out-Null

$downloads = @{
    "chriskohlhoff-asio-asio-1-32-0.tar.gz" = "https://github.com/chriskohlhoff/asio/archive/asio-1-32-0.tar.gz"
    "json-3.12.0.tar.xz" = "https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz"
    "catchorg-Catch2-v3.15.1.tar.gz" = "https://github.com/catchorg/Catch2/archive/v3.15.1.tar.gz"
    "libsdl-org-SDL-release-2.32.10.tar.gz" = "https://github.com/libsdl-org/SDL/archive/release-2.32.10.tar.gz"
    "fmtlib-fmt-12.2.0.tar.gz" = "https://github.com/fmtlib/fmt/archive/12.2.0.tar.gz"
    "gabime-spdlog-v1.17.0.tar.gz" = "https://github.com/gabime/spdlog/archive/v1.17.0.tar.gz"
    "sqlite-autoconf-3530200.tar.gz" = "https://sqlite.org/2026/sqlite-autoconf-3530200.tar.gz"
    "ocornut-imgui-v1.92.8-docking.tar.gz" = "https://github.com/ocornut/imgui/archive/v1.92.8-docking.tar.gz"
}

foreach ($entry in $downloads.GetEnumerator()) {
    $path = Join-Path $CacheDir $entry.Key
    if (-not (Test-Path $path)) {
        Write-Host "Downloading $($entry.Key)..."
        Invoke-WebRequest -Uri $entry.Value -OutFile $path -UseBasicParsing
    }
    else {
        Write-Host "Exists: $($entry.Key)"
    }
}

Write-Host "Third-party cache ready at $CacheDir"

$imguiSrc = Join-Path (Split-Path $CacheDir -Parent) "imgui-src"
$imguiTar = Join-Path $CacheDir "ocornut-imgui-v1.92.8-docking.tar.gz"
if ((Test-Path $imguiTar) -and -not (Test-Path (Join-Path $imguiSrc "backends\imgui_impl_sdl2.cpp"))) {
    Write-Host "Extracting ImGui backends..."
    $temp = Join-Path (Split-Path $CacheDir -Parent) "imgui-extract"
    if (Test-Path $temp) { Remove-Item -Recurse -Force $temp }
    New-Item -ItemType Directory -Force -Path $temp | Out-Null
    tar -xf $imguiTar -C $temp
    $extracted = Get-ChildItem $temp -Directory | Select-Object -First 1
    if (Test-Path $imguiSrc) { Remove-Item -Recurse -Force $imguiSrc }
    Move-Item $extracted.FullName $imguiSrc
    Remove-Item -Recurse -Force $temp
}

Write-Host "Done."
