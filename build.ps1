# RetroRacer - One-Click Build Script for Windows PowerShell
# Builds for any platform using Docker (no SDK installation required)
#
# Usage: .\build.ps1 <platform>
#   platform: dreamcast, psx, ps2, ps3, xbox, native, all

param(
    [Parameter(Position=0)]
    [ValidateSet('native', 'dreamcast', 'dc', 'psx', 'ps1', 'ps2', 'ps3', 'xbox', 'all', 'clean', 'help')]
    [string]$Platform = 'help'
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

function Write-Banner {
    Write-Host ""
    Write-Host "  ____      _             ____                      " -ForegroundColor Cyan
    Write-Host " |  _ \ ___| |_ _ __ ___ |  _ \ __ _  ___ ___ _ __  " -ForegroundColor Cyan
    Write-Host " | |_) / _ \ __| '__/ _ \| |_) / _`` |/ __/ _ \ '__| " -ForegroundColor Cyan
    Write-Host " |  _ <  __/ |_| | | (_) |  _ < (_| | (_|  __/ |    " -ForegroundColor Cyan
    Write-Host " |_| \_\___|\__|_|  \___/|_| \_\__,_|\___\___|_|    " -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Multi-Platform Build System - Windows PowerShell" -ForegroundColor Blue
    Write-Host ""
}

function Write-Info($msg) { Write-Host "[INFO] $msg" -ForegroundColor Blue }
function Write-Success($msg) { Write-Host "[OK] $msg" -ForegroundColor Green }
function Write-Error($msg) { Write-Host "[ERROR] $msg" -ForegroundColor Red }
function Write-Warn($msg) { Write-Host "[WARN] $msg" -ForegroundColor Yellow }

function Test-Docker {
    try {
        docker --version | Out-Null
        docker info | Out-Null
        return $true
    } catch {
        Write-Error "Docker is not installed or not running"
        Write-Host "Please install Docker Desktop: https://docs.docker.com/desktop/windows/"
        return $false
    }
}

function Build-Native {
    Write-Info "Building native Windows binary..."

    # Try MinGW first
    $mingw = Get-Command mingw32-make -ErrorAction SilentlyContinue
    if ($mingw) {
        & mingw32-make -f Makefile.native
        Write-Success "Built: retroracer.exe"
        return
    }

    # Try MSVC
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $vsPath = & $vsWhere -latest -property installationPath
        $vcvars = "$vsPath\VC\Auxiliary\Build\vcvars64.bat"
        if (Test-Path $vcvars) {
            cmd /c "`"$vcvars`" && cl /Fe:retroracer.exe src\*.c /I include"
            Write-Success "Built: retroracer.exe"
            return
        }
    }

    Write-Error "No compiler found. Install MinGW or Visual Studio."
}

function Build-Platform($platform, $dockerfile, $makefile, $output) {
    Write-Info "Building for $platform..."

    if (-not (Test-Docker)) { return $false }

    $tag = "retroracer-$($platform.ToLower())"

    # Build Docker image
    docker build -t $tag -f $dockerfile .
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Docker build failed for $platform"
        return $false
    }

    # Create output directory
    New-Item -ItemType Directory -Force -Path "output" | Out-Null

    # Run build in container
    $mountPath = "$ScriptDir\output".Replace('\', '/')
    docker run --rm -v "${mountPath}:/output" $tag sh -c "make -f $makefile && cp $output /output/ 2>/dev/null || true"

    Write-Success "Built: output\$output"
    return $true
}

function Build-Dreamcast {
    Build-Platform "Dreamcast" "scripts/Dockerfile.dreamcast" "Makefile" "retroracer.elf"
}

function Build-PSX {
    Build-Platform "PSX" "Dockerfile.psx" "Makefile.psx" "retroracer.exe"
}

function Build-PS2 {
    Build-Platform "PS2" "Dockerfile.ps2" "Makefile.ps2" "retroracer.elf"
}

function Build-PS3 {
    Build-Platform "PS3" "Dockerfile.ps3" "Makefile.ps3" "retroracer.pkg"
}

function Build-Xbox {
    Build-Platform "Xbox" "Dockerfile.xbox" "Makefile.xbox" "retroracer.xbe"
}

function Build-All {
    Write-Info "Building for ALL platforms..."

    $results = @{}

    try { Build-Dreamcast; $results["Dreamcast"] = "OK" } catch { $results["Dreamcast"] = "FAILED" }
    try { Build-PSX; $results["PSX"] = "OK" } catch { $results["PSX"] = "FAILED" }
    try { Build-PS2; $results["PS2"] = "OK" } catch { $results["PS2"] = "FAILED" }
    try { Build-PS3; $results["PS3"] = "OK" } catch { $results["PS3"] = "FAILED" }
    try { Build-Xbox; $results["Xbox"] = "OK" } catch { $results["Xbox"] = "FAILED" }

    Write-Host ""
    Write-Host "=== Build Results ===" -ForegroundColor Cyan
    foreach ($key in $results.Keys) {
        $color = if ($results[$key] -eq "OK") { "Green" } else { "Red" }
        Write-Host "  ${key}: $($results[$key])" -ForegroundColor $color
    }
    Write-Host ""
    Write-Host "Output files in: .\output\"
    Get-ChildItem output -ErrorAction SilentlyContinue
}

function Clean-All {
    Write-Info "Cleaning all build artifacts..."

    Remove-Item -Recurse -Force output -ErrorAction SilentlyContinue

    @("retroracer-dc", "retroracer-psx", "retroracer-ps2", "retroracer-ps3", "retroracer-xbox") | ForEach-Object {
        docker rmi $_ 2>$null
    }

    Write-Success "Clean complete"
}

function Show-Help {
    Write-Host "Usage: .\build.ps1 <platform>"
    Write-Host ""
    Write-Host "Platforms:"
    Write-Host "  native     Build for Windows (requires MinGW/MSVC)"
    Write-Host "  dreamcast  Build for Sega Dreamcast"
    Write-Host "  psx        Build for PlayStation 1"
    Write-Host "  ps2        Build for PlayStation 2"
    Write-Host "  ps3        Build for PlayStation 3"
    Write-Host "  xbox       Build for Original Xbox"
    Write-Host "  all        Build for ALL platforms"
    Write-Host "  clean      Remove all build artifacts"
    Write-Host ""
    Write-Host "Examples:"
    Write-Host "  .\build.ps1 ps2      # Build PS2 version"
    Write-Host "  .\build.ps1 all      # Build everything"
    Write-Host ""
    Write-Host "Note: Requires Docker Desktop installed and running"
}

# Main
Write-Banner

switch ($Platform) {
    'native' { Build-Native }
    'dreamcast' { Build-Dreamcast }
    'dc' { Build-Dreamcast }
    'psx' { Build-PSX }
    'ps1' { Build-PSX }
    'ps2' { Build-PS2 }
    'ps3' { Build-PS3 }
    'xbox' { Build-Xbox }
    'all' { Build-All }
    'clean' { Clean-All }
    'help' { Show-Help }
    default { Show-Help }
}
