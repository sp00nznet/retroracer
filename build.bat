@echo off
REM RetroRacer - Windows Build Script
REM
REM Usage: build.bat [target]
REM   Targets: all, clean, cdi, shell
REM
REM This script uses Docker Desktop for building.
REM If you prefer WSL2, run: scripts\windows\setup-wsl.ps1
REM

call "%~dp0scripts\windows\docker-build.bat" %*
