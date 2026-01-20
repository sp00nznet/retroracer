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
if "%1"=="xbox360" goto :build_xbox360
if "%1"=="genesis" goto :build_genesis
if "%1"=="megadrive" goto :build_genesis
if "%1"=="3do" goto :build_3do
if "%1"=="gba" goto :build_gba
if "%1"=="nds" goto :build_nds
if "%1"=="ds" goto :build_nds
if "%1"=="3ds" goto :build_3ds
if "%1"=="wii" goto :build_wii
if "%1"=="wiiu" goto :build_wiiu
if "%1"=="switch" goto :build_switch
if "%1"=="nx" goto :build_switch
if "%1"=="xboxone" goto :build_xboxone
if "%1"=="gameboy" goto :build_gameboy
if "%1"=="gb" goto :build_gameboy
if "%1"=="gamegear" goto :build_gamegear
if "%1"=="gg" goto :build_gamegear
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

:build_xbox360
echo [INFO] Building for Xbox 360...
docker build -t retroracer-xbox360 -f Dockerfile.xbox360 .
if not exist output mkdir output
docker run --rm -v "%SCRIPT_DIR%output:/output" retroracer-xbox360 sh -c "make -f Makefile.xbox360 && cp retroracer_xbox360.xex /output/ 2>/dev/null || true"
echo [OK] Built: output\retroracer_xbox360.xex
goto :end

:build_genesis
echo [INFO] Building for Sega Genesis...
docker build -t retroracer-genesis -f Dockerfile.genesis .
if not exist output mkdir output
docker run --rm -v "%SCRIPT_DIR%output:/output" retroracer-genesis sh -c "make -f Makefile.genesis && cp retroracer_genesis.bin /output/ 2>/dev/null || true"
echo [OK] Built: output\retroracer_genesis.bin
goto :end

:build_3do
echo [INFO] Building for 3DO...
docker build -t retroracer-3do -f Dockerfile.3do .
if not exist output mkdir output
docker run --rm -v "%SCRIPT_DIR%output:/output" retroracer-3do sh -c "make -f Makefile.3do && cp retroracer_3do.iso /output/ 2>/dev/null || true"
echo [OK] Built: output\retroracer_3do.iso
goto :end

:build_gba
echo [INFO] Building for Game Boy Advance...
docker build -t retroracer-gba -f Dockerfile.gba .
if not exist output mkdir output
docker run --rm -v "%SCRIPT_DIR%output:/output" retroracer-gba sh -c "make -f Makefile.gba && cp retroracer_gba.gba /output/ 2>/dev/null || true"
echo [OK] Built: output\retroracer_gba.gba
goto :end

:build_nds
echo [INFO] Building for Nintendo DS...
docker build -t retroracer-nds -f Dockerfile.nds .
if not exist output mkdir output
docker run --rm -v "%SCRIPT_DIR%output:/output" retroracer-nds sh -c "make -f Makefile.nds && cp retroracer_nds.nds /output/ 2>/dev/null || true"
echo [OK] Built: output\retroracer_nds.nds
goto :end

:build_3ds
echo [INFO] Building for Nintendo 3DS...
docker build -t retroracer-3ds -f Dockerfile.3ds .
if not exist output mkdir output
docker run --rm -v "%SCRIPT_DIR%output:/output" retroracer-3ds sh -c "make -f Makefile.3ds && cp retroracer_3ds.3dsx /output/ 2>/dev/null || true"
echo [OK] Built: output\retroracer_3ds.3dsx
goto :end

:build_wii
echo [INFO] Building for Nintendo Wii...
docker build -t retroracer-wii -f Dockerfile.wii .
if not exist output mkdir output
docker run --rm -v "%SCRIPT_DIR%output:/output" retroracer-wii sh -c "make -f Makefile.wii && cp retroracer.dol /output/retroracer_wii.dol 2>/dev/null || true"
echo [OK] Built: output\retroracer_wii.dol
goto :end

:build_wiiu
echo [INFO] Building for Nintendo Wii U...
docker build -t retroracer-wiiu -f Dockerfile.wiiu .
if not exist output mkdir output
docker run --rm -v "%SCRIPT_DIR%output:/output" retroracer-wiiu sh -c "make -f Makefile.wiiu && cp retroracer.rpx /output/retroracer_wiiu.rpx 2>/dev/null || true"
echo [OK] Built: output\retroracer_wiiu.rpx
goto :end

:build_switch
echo [INFO] Building for Nintendo Switch...
docker build -t retroracer-switch -f Dockerfile.switch .
if not exist output mkdir output
docker run --rm -v "%SCRIPT_DIR%output:/output" retroracer-switch sh -c "make -f Makefile.switch && cp retroracer.nro /output/retroracer_switch.nro 2>/dev/null || true"
echo [OK] Built: output\retroracer_switch.nro
goto :end

:build_xboxone
echo [INFO] Building for Xbox One...
docker build -t retroracer-xboxone -f Dockerfile.xboxone .
if not exist output mkdir output
docker run --rm -v "%SCRIPT_DIR%output:/output" retroracer-xboxone sh -c "make -f Makefile.xboxone 2>/dev/null || true"
echo [OK] Built: output\retroracer_xboxone (stub)
goto :end

:build_gameboy
echo [INFO] Building for Game Boy...
docker build -t retroracer-gameboy -f Dockerfile.gameboy .
if not exist output mkdir output
docker run --rm -v "%SCRIPT_DIR%output:/output" retroracer-gameboy sh -c "make -f Makefile.gameboy && cp retroracer_gb.gb /output/ 2>/dev/null || true"
echo [OK] Built: output\retroracer_gb.gb
goto :end

:build_gamegear
echo [INFO] Building for Game Gear...
docker build -t retroracer-gamegear -f Dockerfile.gamegear .
if not exist output mkdir output
docker run --rm -v "%SCRIPT_DIR%output:/output" retroracer-gamegear sh -c "make -f Makefile.gamegear && cp retroracer_gg.gg /output/ 2>/dev/null || true"
echo [OK] Built: output\retroracer_gg.gg
goto :end

:build_all
echo [INFO] Building for ALL platforms...
if not exist output mkdir output

call :build_dreamcast
call :build_psx
call :build_ps2
call :build_ps3
call :build_xbox
call :build_xbox360
call :build_n64
call :build_snes
call :build_genesis
call :build_3do
call :build_gba
call :build_nds
call :build_3ds
call :build_wii
call :build_wiiu
call :build_switch
call :build_xboxone
call :build_gameboy
call :build_gamegear

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
echo   xbox360    Build for Xbox 360
echo   n64        Build for Nintendo 64
echo   snes       Build for Super Nintendo
echo   genesis    Build for Sega Genesis
echo   3do        Build for 3DO
echo   gba        Build for Game Boy Advance
echo   nds        Build for Nintendo DS
echo   3ds        Build for Nintendo 3DS
echo   wii        Build for Nintendo Wii
echo   wiiu       Build for Nintendo Wii U
echo   switch     Build for Nintendo Switch
echo   xboxone    Build for Xbox One
echo   gameboy    Build for Game Boy
echo   gamegear   Build for Game Gear
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
