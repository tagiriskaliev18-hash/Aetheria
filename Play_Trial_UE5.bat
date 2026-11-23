@echo off
setlocal
set "PROJ_DIR=%~dp0"
if "%PROJ_DIR:~-1%"=="\" set "PROJ_DIR=%PROJ_DIR:~0,-1%"
if exist "%PROJ_DIR%\Build\Windows\Aetheria.exe" (
    start "" /D "%PROJ_DIR%\Build\Windows" "%PROJ_DIR%\Build\Windows\Aetheria.exe" -WindowedFullscreen -ExecCmds="r.ScreenPercentage 100, r.MaxAnisotropy 16, r.VT.MaxAnisotropy 16, r.ContactShadows 1, r.CapsuleShadows 1, r.Tonemapper.Sharpen 0.65, r.Shadow.Virtual.ResolutionLodBiasDirectional -1.75"
    exit /b 0
)
if exist "C:\Games\AETHERIA\Aetheria.exe" (
    start "" /D "C:\Games\AETHERIA" "C:\Games\AETHERIA\Aetheria.exe" -WindowedFullscreen -ExecCmds="r.ScreenPercentage 100, r.MaxAnisotropy 16, r.VT.MaxAnisotropy 16, r.ContactShadows 1, r.CapsuleShadows 1, r.Tonemapper.Sharpen 0.65, r.Shadow.Virtual.ResolutionLodBiasDirectional -1.75"
    exit /b 0
)
start "" /D "%PROJ_DIR%" "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%PROJ_DIR%\Aetheria.uproject" /Engine/Maps/Entry -game -unattended -ResX=1920 -ResY=1080 -Windowed -ExecCmds="r.ScreenPercentage 100, r.MaxAnisotropy 16, r.VT.MaxAnisotropy 16, r.ContactShadows 1, r.CapsuleShadows 1, r.Tonemapper.Sharpen 0.65, r.Shadow.Virtual.ResolutionLodBiasDirectional -1.75"
exit /b 0