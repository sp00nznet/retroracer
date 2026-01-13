@echo off
REM RetroRacer - Windows Setup Script
REM One-click setup for Dreamcast development on Windows
REM
REM This will install Docker Desktop if not present
REM Run as Administrator for best results
REM

echo.
echo ========================================
echo   RetroRacer - Windows Setup
echo ========================================
echo.

REM Check if Docker is available
docker --version >nul 2>&1
if errorlevel 1 (
    echo Docker is not installed.
    echo.
    echo Choose an option:
    echo   1. Install Docker Desktop (recommended, easier)
    echo   2. Setup WSL2 with full toolchain (advanced)
    echo   3. Exit
    echo.
    set /p choice="Enter choice (1-3): "

    if "%choice%"=="1" (
        echo.
        echo Installing Docker Desktop...
        echo This requires Administrator privileges.
        echo.
        powershell -ExecutionPolicy Bypass -File "%~dp0install-docker.ps1"
    ) else if "%choice%"=="2" (
        echo.
        echo Setting up WSL2...
        echo This requires Administrator privileges.
        echo.
        powershell -ExecutionPolicy Bypass -File "%~dp0setup-wsl.ps1"
    ) else (
        echo Exiting.
        exit /b 0
    )
) else (
    echo Docker is installed!
    docker --version
    echo.

    REM Check if Docker daemon is running
    docker info >nul 2>&1
    if errorlevel 1 (
        echo Docker Desktop is not running.
        echo.
        echo Please start Docker Desktop from the Start menu,
        echo wait for it to fully start, then run this again.
        echo.
        pause
        exit /b 1
    )

    echo Docker is running and ready!
    echo.
    echo You can now build RetroRacer:
    echo   build.bat
    echo.
    echo Or open the project folder and run:
    echo   scripts\windows\docker-build.bat
    echo.
    pause
)
