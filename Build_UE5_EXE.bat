@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul

set "PROJ_DIR=%~dp0"
if "%PROJ_DIR:~-1%"=="\" set "PROJ_DIR=%PROJ_DIR:~0,-1%"
set "UPROJECT=%PROJ_DIR%\Aetheria.uproject"
set "OUT_DIR=%PROJ_DIR%\Build"

echo ============================================================================
echo   AETHERIA (Unreal Engine 5) - Build and Package Standalone Windows .EXE
echo ============================================================================

set "UE_ROOT="

REM 1. Auto-detect any UE 5.x install path from Epic Games Launcher manifest (works on C:, D:, custom folders)
for /f "usebackq delims=" %%I in (`powershell -NoProfile -Command "$f='C:\ProgramData\Epic\UnrealEngineLauncher\LauncherInstalled.dat'; if(Test-Path $f){ $j=Get-Content $f -Raw | ConvertFrom-Json; $u=$j.InstallationList | Where-Object { $_.AppName -like 'UE_5*' } | Sort-Object AppName -Descending | Select-Object -First 1; if($u -and (Test-Path (Join-Path $u.InstallLocation 'Engine\Build\BatchFiles\RunUAT.bat'))){ $u.InstallLocation } }"`) do (
    if not defined UE_ROOT set "UE_ROOT=%%I"
)

REM 2. Fallback scan of standard directories
for %%V in (5.8 5.7 5.6 5.5 5.4 5.3) do (
    for %%D in (
        "C:\Program Files\Epic Games\UE_%%V"
        "D:\Program Files\Epic Games\UE_%%V"
        "D:\Epic Games\UE_%%V"
        "C:\Epic Games\UE_%%V"
        "D:\UE_%%V"
        "C:\UE_%%V"
    ) do (
        if not defined UE_ROOT (
            if exist "%%~D\Engine\Build\BatchFiles\RunUAT.bat" (
                set "UE_ROOT=%%~D"
            )
        )
    )
)

REM 3. Fallback Registry lookup (machine-wide, then per-user source builds)
for %%V in (5.8 5.7 5.6 5.5 5.4 5.3) do (
    if not defined UE_ROOT (
        for /f "tokens=2*" %%A in ('reg query "HKLM\SOFTWARE\Epic Games\Unreal Engine\%%V" /v InstalledDirectory 2^>nul') do (
            if exist "%%B\Engine\Build\BatchFiles\RunUAT.bat" set "UE_ROOT=%%B"
        )
    )
    if not defined UE_ROOT (
        for /f "tokens=2*" %%A in ('reg query "HKCU\SOFTWARE\Epic Games\Unreal Engine\Builds" /v %%V 2^>nul') do (
            if exist "%%B\Engine\Build\BatchFiles\RunUAT.bat" set "UE_ROOT=%%B"
        )
    )
)

if not defined UE_ROOT (
    echo [ERROR] Unreal Engine 5.3 - 5.8 is not installed on this PC.
    echo Open Epic Games Launcher and install Unreal Engine 5.8, or run Setup_UE5_Toolchain.ps1 first.
    exit /b 1
)

echo [INFO] Found Unreal Engine at: "%UE_ROOT%"
echo [INFO] Project: "%UPROJECT%"

echo.
echo [1/3] Compiling AetheriaEditor C++ modules (Win64 Development)...
call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" AetheriaEditor Win64 Development -Project="%UPROJECT%" -WaitMutex -FromMsBuild
if errorlevel 1 (
    echo [ERROR] C++ Editor build failed. Ensure Visual Studio 2022 C++ Game Development workload is installed.
    exit /b 1
)

echo.
echo [2/3] Baking Mind procedural shaders into /Game/Materials...
"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%UPROJECT%" -run=AetheriaShaderBake -unattended -nop4 -nosplash -NullRHI
if errorlevel 1 (
    echo [WARN] Shader bake commandlet exited with non-zero code; continuing with fallback materials.
)

echo.
echo [3/3] Cooking and packaging standalone Windows .exe via RunUAT BuildCookRun...
call "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
    -project="%UPROJECT%" ^
    -noP4 ^
    -platform=Win64 ^
    -clientconfig=Development ^
    -cook ^ ^
    -map=/Engine/Maps/Entry+/Game/Maps/AetheriaWorlds/ArtistMenu+/Game/Maps/AetheriaWorlds/MenuCourtyard+/Game/Maps/AetheriaWorlds/World_Bastion+/Game/Maps/AetheriaWorlds/World_Ruins+/Game/Maps/AetheriaWorlds/World_Sanctum+/Game/Maps/AetheriaWorlds/World_Void+/Game/Maps/AetheriaWorlds/World_VoidShell+/Game/Maps/AetheriaWorld ^
    -build ^
    -stage ^
    -pak ^
    -archive ^
    -archivedirectory="%OUT_DIR%" ^
    -utf8output
if errorlevel 1 (
    echo [ERROR] RunUAT BuildCookRun packaging failed.
    exit /b 1
)

REM Stage RawAssets next to the exe: the HUD/anim binding loads fonts, UI
REM brushes and sounds from <exe dir>\RawAssets at runtime.
robocopy "%PROJ_DIR%\RawAssets" "%OUT_DIR%\Windows\RawAssets" /E /NFL /NDL /NJH /NJS >nul
if errorlevel 8 (
    echo [WARN] RawAssets staging reported errors; UI may fall back to defaults.
)

echo.
echo ============================================================================
echo   SUCCESS! Standalone executable ready:
echo   "%OUT_DIR%\Windows\Aetheria.exe"
echo ============================================================================
exit /b 0
