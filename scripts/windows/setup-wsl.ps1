# RetroRacer - Windows WSL2 Setup Script
# Sets up Windows Subsystem for Linux with KallistiOS for Dreamcast development
#
# Run as Administrator:
#   powershell -ExecutionPolicy Bypass -File scripts\windows\setup-wsl.ps1
#

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  RetroRacer - Windows Development Setup" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "This script will:" -ForegroundColor White
Write-Host "  1. Enable WSL2 (Windows Subsystem for Linux)" -ForegroundColor Gray
Write-Host "  2. Install Ubuntu" -ForegroundColor Gray
Write-Host "  3. Set up KallistiOS toolchain" -ForegroundColor Gray
Write-Host ""

# Check if running as administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "ERROR: This script must be run as Administrator" -ForegroundColor Red
    Write-Host ""
    Write-Host "Right-click PowerShell and select 'Run as Administrator'" -ForegroundColor Yellow
    Write-Host "Then run: powershell -ExecutionPolicy Bypass -File scripts\windows\setup-wsl.ps1" -ForegroundColor Yellow
    exit 1
}

# Check Windows version
$winVer = [System.Environment]::OSVersion.Version
if ($winVer.Build -lt 19041) {
    Write-Host "ERROR: Windows 10 version 2004 or later required for WSL2" -ForegroundColor Red
    Write-Host "Current build: $($winVer.Build)" -ForegroundColor Yellow
    Write-Host "Please update Windows and try again." -ForegroundColor Yellow
    exit 1
}

Write-Host "Windows version check passed (Build $($winVer.Build))" -ForegroundColor Green
Write-Host ""

# Enable WSL
Write-Host "Enabling WSL..." -ForegroundColor Yellow
$wslFeature = Get-WindowsOptionalFeature -Online -FeatureName Microsoft-Windows-Subsystem-Linux
if ($wslFeature.State -ne "Enabled") {
    Enable-WindowsOptionalFeature -Online -FeatureName Microsoft-Windows-Subsystem-Linux -NoRestart
    Write-Host "WSL enabled" -ForegroundColor Green
} else {
    Write-Host "WSL already enabled" -ForegroundColor Green
}

# Enable Virtual Machine Platform
Write-Host "Enabling Virtual Machine Platform..." -ForegroundColor Yellow
$vmFeature = Get-WindowsOptionalFeature -Online -FeatureName VirtualMachinePlatform
if ($vmFeature.State -ne "Enabled") {
    Enable-WindowsOptionalFeature -Online -FeatureName VirtualMachinePlatform -NoRestart
    Write-Host "Virtual Machine Platform enabled" -ForegroundColor Green
} else {
    Write-Host "Virtual Machine Platform already enabled" -ForegroundColor Green
}

# Check if reboot needed
if ($wslFeature.State -ne "Enabled" -or $vmFeature.State -ne "Enabled") {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Yellow
    Write-Host "  REBOOT REQUIRED" -ForegroundColor Yellow
    Write-Host "========================================" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Please restart your computer, then run this script again." -ForegroundColor White
    Write-Host ""
    $restart = Read-Host "Restart now? (y/N)"
    if ($restart -eq "y" -or $restart -eq "Y") {
        Restart-Computer
    }
    exit 0
}

# Set WSL2 as default
Write-Host "Setting WSL2 as default..." -ForegroundColor Yellow
wsl --set-default-version 2 2>$null
if ($LASTEXITCODE -ne 0) {
    Write-Host "Downloading WSL2 kernel update..." -ForegroundColor Yellow
    $kernelUrl = "https://wslstorestorage.blob.core.windows.net/wslblob/wsl_update_x64.msi"
    $kernelPath = "$env:TEMP\wsl_update_x64.msi"
    Invoke-WebRequest -Uri $kernelUrl -OutFile $kernelPath
    Start-Process msiexec.exe -Wait -ArgumentList "/i $kernelPath /quiet"
    Remove-Item $kernelPath
    wsl --set-default-version 2
}
Write-Host "WSL2 set as default" -ForegroundColor Green

# Install Ubuntu
Write-Host ""
Write-Host "Checking for Ubuntu installation..." -ForegroundColor Yellow
$ubuntuInstalled = wsl -l -q 2>$null | Select-String -Pattern "Ubuntu"
if (-not $ubuntuInstalled) {
    Write-Host "Installing Ubuntu (this may take a few minutes)..." -ForegroundColor Yellow
    wsl --install -d Ubuntu --no-launch
    Write-Host "Ubuntu installed" -ForegroundColor Green
    Write-Host ""
    Write-Host "IMPORTANT: After this script completes:" -ForegroundColor Yellow
    Write-Host "  1. Launch Ubuntu from the Start menu" -ForegroundColor White
    Write-Host "  2. Create your Linux username and password" -ForegroundColor White
    Write-Host "  3. Run: ./scripts/setup-kos.sh" -ForegroundColor White
} else {
    Write-Host "Ubuntu already installed" -ForegroundColor Green
}

# Create Windows batch file for easy building
$projectPath = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))
$buildBat = @"
@echo off
REM RetroRacer - Build via WSL
REM Usage: build.bat [target]

set TARGET=%1
if "%TARGET%"=="" set TARGET=all

echo Building RetroRacer (%TARGET%)...
wsl bash -c "cd /mnt/c%CD:\=/% && source scripts/env.sh 2>/dev/null; ./scripts/build.sh %TARGET%"
"@

$buildBatPath = Join-Path $projectPath "build.bat"
$buildBat | Out-File -FilePath $buildBatPath -Encoding ASCII
Write-Host ""
Write-Host "Created build.bat for easy building" -ForegroundColor Green

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "  Setup Complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor White
Write-Host "  1. Launch 'Ubuntu' from the Start menu" -ForegroundColor Gray
Write-Host "  2. Create your Linux username/password when prompted" -ForegroundColor Gray
Write-Host "  3. Navigate to the project:" -ForegroundColor Gray
Write-Host "     cd /mnt/c/path/to/retroracer" -ForegroundColor Cyan
Write-Host "  4. Run the KallistiOS setup:" -ForegroundColor Gray
Write-Host "     ./scripts/setup-kos.sh" -ForegroundColor Cyan
Write-Host "  5. Build the game:" -ForegroundColor Gray
Write-Host "     source scripts/env.sh && ./scripts/build.sh" -ForegroundColor Cyan
Write-Host ""
Write-Host "Or use Docker Desktop (easier):" -ForegroundColor White
Write-Host "  1. Install Docker Desktop from docker.com" -ForegroundColor Gray
Write-Host "  2. Run: .\scripts\windows\docker-build.bat" -ForegroundColor Cyan
Write-Host ""
