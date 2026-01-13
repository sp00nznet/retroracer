# RetroRacer - Docker Desktop Installer for Windows
# Downloads and installs Docker Desktop for easy Dreamcast development
#
# Run as Administrator:
#   powershell -ExecutionPolicy Bypass -File scripts\windows\install-docker.ps1
#

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Docker Desktop Installer" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check if running as administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "ERROR: This script must be run as Administrator" -ForegroundColor Red
    Write-Host ""
    Write-Host "Right-click PowerShell and select 'Run as Administrator'" -ForegroundColor Yellow
    exit 1
}

# Check if Docker is already installed
$dockerPath = Get-Command docker -ErrorAction SilentlyContinue
if ($dockerPath) {
    Write-Host "Docker is already installed!" -ForegroundColor Green
    docker --version
    Write-Host ""
    Write-Host "You can now build RetroRacer:" -ForegroundColor White
    Write-Host "  .\build.bat" -ForegroundColor Cyan
    exit 0
}

# Check Windows version for WSL2 backend
$winVer = [System.Environment]::OSVersion.Version
$useWSL2 = $winVer.Build -ge 19041

if ($useWSL2) {
    Write-Host "Windows 10 2004+ detected - will use WSL2 backend" -ForegroundColor Green
} else {
    Write-Host "Older Windows detected - will use Hyper-V backend" -ForegroundColor Yellow
}

# Download Docker Desktop
Write-Host ""
Write-Host "Downloading Docker Desktop..." -ForegroundColor Yellow
$dockerUrl = "https://desktop.docker.com/win/main/amd64/Docker%20Desktop%20Installer.exe"
$installerPath = "$env:TEMP\DockerDesktopInstaller.exe"

try {
    $ProgressPreference = 'SilentlyContinue'  # Speed up download
    Invoke-WebRequest -Uri $dockerUrl -OutFile $installerPath -UseBasicParsing
    $ProgressPreference = 'Continue'
    Write-Host "Download complete" -ForegroundColor Green
} catch {
    Write-Host "ERROR: Failed to download Docker Desktop" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    Write-Host ""
    Write-Host "Please download manually from:" -ForegroundColor Yellow
    Write-Host "  https://www.docker.com/products/docker-desktop" -ForegroundColor Cyan
    exit 1
}

# Enable required Windows features
Write-Host ""
Write-Host "Enabling Windows features..." -ForegroundColor Yellow

if ($useWSL2) {
    # Enable WSL
    $wslFeature = Get-WindowsOptionalFeature -Online -FeatureName Microsoft-Windows-Subsystem-Linux
    if ($wslFeature.State -ne "Enabled") {
        Write-Host "  Enabling WSL..." -ForegroundColor Gray
        Enable-WindowsOptionalFeature -Online -FeatureName Microsoft-Windows-Subsystem-Linux -NoRestart -WarningAction SilentlyContinue | Out-Null
    }

    # Enable Virtual Machine Platform
    $vmFeature = Get-WindowsOptionalFeature -Online -FeatureName VirtualMachinePlatform
    if ($vmFeature.State -ne "Enabled") {
        Write-Host "  Enabling Virtual Machine Platform..." -ForegroundColor Gray
        Enable-WindowsOptionalFeature -Online -FeatureName VirtualMachinePlatform -NoRestart -WarningAction SilentlyContinue | Out-Null
    }
} else {
    # Enable Hyper-V for older Windows
    $hyperV = Get-WindowsOptionalFeature -Online -FeatureName Microsoft-Hyper-V-All -ErrorAction SilentlyContinue
    if ($hyperV -and $hyperV.State -ne "Enabled") {
        Write-Host "  Enabling Hyper-V..." -ForegroundColor Gray
        Enable-WindowsOptionalFeature -Online -FeatureName Microsoft-Hyper-V-All -NoRestart -WarningAction SilentlyContinue | Out-Null
    }
}

Write-Host "Windows features configured" -ForegroundColor Green

# Install Docker Desktop
Write-Host ""
Write-Host "Installing Docker Desktop..." -ForegroundColor Yellow
Write-Host "This may take a few minutes..." -ForegroundColor Gray

$installArgs = "install --quiet --accept-license"
if ($useWSL2) {
    $installArgs += " --backend=wsl-2"
}

Start-Process -FilePath $installerPath -ArgumentList $installArgs -Wait

# Clean up installer
Remove-Item $installerPath -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "  Installation Complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "IMPORTANT: You need to:" -ForegroundColor Yellow
Write-Host "  1. RESTART your computer" -ForegroundColor White
Write-Host "  2. Launch Docker Desktop from the Start menu" -ForegroundColor White
Write-Host "  3. Wait for Docker to fully start (whale icon in system tray)" -ForegroundColor White
Write-Host "  4. Run: .\build.bat" -ForegroundColor Cyan
Write-Host ""

$restart = Read-Host "Restart now? (y/N)"
if ($restart -eq "y" -or $restart -eq "Y") {
    Restart-Computer
}
