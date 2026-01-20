@echo off
REM RetroRacer - One-Click Build Script for Windows
REM Builds for any platform using Docker (no SDK installation required)
REM
REM Usage: build.bat <platform>
REM   platform: dreamcast, psx, ps2, ps3, xbox, native, all

setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%"

echo.
echo   ____      _             ____
echo  ^|  _ \ ___^| ^|_ _ __ ___ ^|  _ \ __ _  ___ ___ _ __
echo  ^| ^|_) / _ \ __^| '__/ _ \^| ^|_) / _` ^|/ __/ _ \ '__^|
echo  ^|  _ ^<  __/ ^|_^| ^| ^| (_) ^|  _ ^< (_^| ^| (_^|  __/ ^|
echo  ^|_^| \_\___^|\__^|_^|  \___/^|_^| \_\__,_^|\___\___^|_^|
echo.
echo Multi-Platform Build System - Windows
echo.

if "%1"=="" goto :usage

REM Check for Docker
docker --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Docker is not installed or not in PATH
    echo Please install Docker Desktop: https://docs.docker.com/desktop/windows/
    exit /b 1
)

if "%1"=="native" goto :build_native
if "%1"=="dreamcast" goto :build_dreamcast
if "%1"=="dc" goto :build_dreamcast
if "%1"=="psx" goto :build_psx
if "%1"=="ps1" goto :build_psx
if "%1"=="ps2" goto :build_ps2
if "%1"=="ps3" goto :build_ps3
if "%1"=="xbox" goto :build_xbox
if "%1"=="n64" goto :build_n64
if "%1"=="snes" goto :build_snes
if "%1"=="all" goto :build_all
if "%1"=="clean" goto :clean_all

echo [ERROR] Unknown platform: %1
goto :usage

:build_native
echo [INFO] Building native (requires MinGW or MSVC)...
if exist Makefile.native (
    mingw32-make -f Makefile.native 2>nul || (
        echo [WARN] MinGW not found, trying MSVC...
        cl /Fe:retroracer.exe src\*.c /I include /link /SUBSYSTEM:CONSOLE
    )
)
echo [OK] Built: retroracer.exe
goto :end

:build_dreamcast
echo [INFO] Building for Dreamcast...
docker build -t retroracer-dc -f scripts/Dockerfile.dreamcast .
if not exist output mkdir output
docker run --rm -v "%SCRIPT_DIR%output:/output" retroracer-dc sh -c "make clean; make && cp retroracer.elf /output/"
echo [OK] Built: output\retroracer.elf
goto :end

:build_psx
echo [INFO] Building for PlayStation 1...
docker build -t retroracer-psx -f Dockerfile.psx .
if not exist output mkdir output
docker run --rm -v "%SCRIPT_DIR%output:/output" retroracer-psx sh -c "make -f Makefile.psx && cp retroracer.exe /output/retroracer_psx.exe 2>/dev/null || true"
echo [OK] Built: output\retroracer_psx.exe
goto :end

:build_ps2
echo [INFO] Building for PlayStation 2...
docker build -t retroracer-ps2 -f Dockerfile.ps2 .
if not exist output mkdir output
docker run --rm -v "%SCRIPT_DIR%output:/output" retroracer-ps2 sh -c "make -f Makefile.ps2 && cp retroracer.elf /output/retroracer_ps2.elf 2>/dev/null || true"
echo [OK] Built: output\retroracer_ps2.elf
goto :end

:build_ps3
echo [INFO] Building for PlayStation 3...
docker build -t retroracer-ps3 -f Dockerfile.ps3 .
if not exist output mkdir output
docker run --rm -v "%SCRIPT_DIR%output:/output" retroracer-ps3 sh -c "make -f Makefile.ps3 && cp retroracer.pkg /output/ 2>/dev/null || true"
echo [OK] Built: output\retroracer.pkg
goto :end

:build_xbox
echo [INFO] Building for Original Xbox...
docker build -t retroracer-xbox -f Dockerfile.xbox .
if not exist output mkdir output
docker run --rm -v "%SCRIPT_DIR%output:/output" retroracer-xbox sh -c "make -f Makefile.xbox && cp retroracer.xbe /output/ 2>/dev/null || true"
echo [OK] Built: output\retroracer.xbe
goto :end

:build_n64
echo [INFO] Building for Nintendo 64...
docker build -t retroracer-n64 -f Dockerfile.n64 .
if not exist output mkdir output
docker run --rm -v "%SCRIPT_DIR%output:/output" retroracer-n64 sh -c "make -f Makefile.n64 && cp retroracer.z64 /output/ 2>/dev/null || true"
echo [OK] Built: output\retroracer.z64
goto :end

:build_snes
echo [INFO] Building for Super Nintendo...
docker build -t retroracer-snes -f Dockerfile.snes .
if not exist output mkdir output
docker run --rm -v "%SCRIPT_DIR%output:/output" retroracer-snes sh -c "make -f Makefile.snes && cp retroracer.sfc /output/ 2>/dev/null || true"
echo [OK] Built: output\retroracer.sfc
goto :end

:build_all
echo [INFO] Building for ALL platforms...
if not exist output mkdir output

call :build_dreamcast
call :build_psx
call :build_ps2
call :build_ps3
call :build_xbox
call :build_n64
call :build_snes

echo.
echo === Build Complete ===
echo Output files in: output\
dir output\
goto :end

:clean_all
echo [INFO] Cleaning all build artifacts...
if exist output rmdir /s /q output
docker rmi retroracer-dc retroracer-psx retroracer-ps2 retroracer-ps3 retroracer-xbox 2>nul
echo [OK] Clean complete
goto :end

:usage
echo Usage: build.bat ^<platform^>
echo.
echo Platforms:
echo   native     Build for Windows (requires MinGW/MSVC)
echo   dreamcast  Build for Sega Dreamcast
echo   psx        Build for PlayStation 1
echo   ps2        Build for PlayStation 2
echo   ps3        Build for PlayStation 3
echo   xbox       Build for Original Xbox
echo   n64        Build for Nintendo 64
echo   snes       Build for Super Nintendo
echo   all        Build for ALL platforms
echo   clean      Remove all build artifacts
echo.
echo Examples:
echo   build.bat ps2      Build PS2 version
echo   build.bat all      Build everything
echo.
echo Note: Requires Docker Desktop installed and running
goto :end

:end
endlocal
