@echo off

cd /d %~dp0

set UE_PATH=%~dp0..\..\Squad\UE5_Upgrade\UnrealEngine\Engine
set PROJECT_PATH=%~dp0ProjectilePerf.uproject
set DEVENV_PATH=D:\VS2022

"%DEVENV_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x64
