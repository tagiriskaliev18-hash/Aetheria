@echo off
setlocal
set "PROJ_DIR=%~dp0"
if "%PROJ_DIR:~-1%"=="\" set "PROJ_DIR=%PROJ_DIR:~0,-1%"
if exist "%PROJ_DIR%\Build\Windows\Aetheria.exe" (
    start "" /D "%PROJ_DIR%\Build\Windows" "%PROJ_DIR%\Build\Windows\Aetheria.exe" -WindowedFullscreen -ExecCmds="r.ScreenPercentage 100, r.MaxAnisotropy 16, r.Tonemapper.Sharpen 0.65"
    exit /b 0
)
start "" /D "%PROJ_DIR%" "C:\Users\tagir\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%PROJ_DIR%\Aetheria.uproject" -game -WindowedFullscreen -ExecCmds="r.ScreenPercentage 100, r.MaxAnisotropy 16, r.Tonemapper.Sharpen 0.65"
