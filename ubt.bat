@echo off

if "%UE_PATH%"=="" (
	echo UE_PATH is not SET. Please run vars.bat first
	exit /b 1
)

if "%PROJECT_PATH%"=="" (
	echo PROJECT_PATH is not SET. Please run vars.bat first
	exit /b 1
)

rem dotnet "%UE_PATH%\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" 
"%UE_PATH%\Build\BatchFiles\Build.bat" %* -project="%PROJECT_PATH%"