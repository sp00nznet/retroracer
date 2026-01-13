@echo off
REM RetroRacer - Docker Build Script for Windows
REM Builds the game using Docker (no local toolchain needed)
REM
REM Prerequisites: Docker Desktop installed and running
REM Download from: https://www.docker.com/products/docker-desktop
REM
REM Usage: docker-build.bat [target]
REM   Targets:
REM     all     - Build the ELF executable (default)
REM     clean   - Clean build artifacts
REM     shell   - Open interactive shell
REM

setlocal enabledelayedexpansion

set "DOCKER_IMAGE=kazade/dreamcast-sdk:latest"
set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=all"

echo.
echo ========================================
echo   RetroRacer - Docker Build (Windows)
echo ========================================
echo.

REM Check if Docker is available
docker --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Docker is not installed or not in PATH
    echo.
    echo Please install Docker Desktop from:
    echo   https://www.docker.com/products/docker-desktop
    echo.
    echo After installation:
    echo   1. Start Docker Desktop
    echo   2. Wait for it to fully start
    echo   3. Run this script again
    echo.
    pause
    exit /b 1
)

REM Check if Docker daemon is running
docker info >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Docker daemon is not running
    echo.
    echo Please start Docker Desktop and wait for it to be ready.
    echo.
    pause
    exit /b 1
)

REM Get the project directory (parent of scripts\windows)
set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%\..\.."
set "PROJECT_DIR=%CD%"

echo Project: %PROJECT_DIR%
echo Target: %TARGET%
echo.

if "%TARGET%"=="shell" (
    echo Opening interactive shell...
    echo Type 'exit' to leave the container.
    echo.
    docker run -it --rm -v "%PROJECT_DIR%:/src" -w /src -e "KOS_BASE=/opt/toolchains/dc/kos" %DOCKER_IMAGE% bash -c "source /opt/toolchains/dc/kos/environ.sh && exec bash"
    goto :end
)

if "%TARGET%"=="clean" (
    echo Cleaning build artifacts...
    docker run --rm -v "%PROJECT_DIR%:/src" -w /src %DOCKER_IMAGE% make clean
    del /q "%PROJECT_DIR%\retroracer.bin" 2>nul
    del /q "%PROJECT_DIR%\1ST_READ.BIN" 2>nul
    del /q "%PROJECT_DIR%\retroracer.iso" 2>nul
    del /q "%PROJECT_DIR%\retroracer.cdi" 2>nul
    echo Clean complete.
    goto :end
)

if "%TARGET%"=="pull" (
    echo Pulling Docker image...
    docker pull %DOCKER_IMAGE%
    goto :end
)

REM Default: build
echo Pulling Docker image (if needed)...
docker pull %DOCKER_IMAGE%

echo.
echo Building RetroRacer...
docker run --rm -v "%PROJECT_DIR%:/src" -w /src -e "KOS_BASE=/opt/toolchains/dc/kos" %DOCKER_IMAGE% bash -c "source /opt/toolchains/dc/kos/environ.sh && make %TARGET%"

if exist "%PROJECT_DIR%\retroracer.elf" (
    echo.
    echo ========================================
    echo   Build Successful!
    echo ========================================
    echo.
    echo Output: retroracer.elf
    dir "%PROJECT_DIR%\retroracer.elf"
    echo.
    echo To create a disc image, run:
    echo   docker-build.bat cdi
    echo.
) else (
    echo.
    echo [ERROR] Build may have failed - retroracer.elf not found
    echo.
)

:end
endlocal
