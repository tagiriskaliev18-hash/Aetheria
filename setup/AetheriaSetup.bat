@echo off
setlocal EnableDelayedExpansion
chcp 65001 >nul
title AETHERIA: Shards of the Chrono-Seal — Setup

echo ============================================================================
echo   AETHERIA: Shards of the Chrono-Seal — Installer
echo   Скачивает игру с GitHub Releases (~15 ГБ) и распаковывает в выбранную
echo   папку. Требуется ~32 ГБ свободного места и интернет.
echo ============================================================================

set "REPO=tagiriskaliev18-hash/Aetheria"
set "REL=v0.1.0-demo"
set "PREFIX=Aetheria-Windows-v0.1.0.7z"
set "DEST=%~dp0AETHERIA"

choice /C YN /M "Установить в !DEST! (Y) или выбрать папку (N)"
if errorlevel 2 (
    set /p DEST="Папка установки: "
)
if not exist "!DEST!" mkdir "!DEST!"

echo.
echo [1/4] Скачиваю 7zr.exe...
curl -sL -o "!DEST!\7zr.exe" "https://raw.githubusercontent.com/%REPO%/main/setup/7zr.exe" || goto :err

echo [2/4] Получаю список томов релиза %REL%...
curl -sL "https://api.github.com/repos/%REPO%/releases" > "%TEMP%\gh_rel.json"
python -c "import json,sys;rels=json.load(open(r'%TEMP%\gh_rel.json'));r=[x for x in rels if x['tag_name']=='%REL%'][0];print('\n'.join(a['browser_download_url'] for a in r['assets']))" > "%TEMP%\gh_parts.txt" 2>nul
if errorlevel 1 powershell -NoProfile -Command "(Get-Content $env:TEMP\gh_rel.json | ConvertFrom-Json | Where-Object tag_name -eq '%REL%').assets.browser_download_url | Set-Content $env:TEMP\gh_parts.txt"
find /C ".7z." "%TEMP%\gh_parts.txt" >nul || (echo Не найдены тома в релизе & goto :err)
set /A N=0
for /f %%U in ('type "%TEMP%\gh_parts.txt"') do set /A N+=1
echo Найдено томов: !N!

echo [3/4] Скачиваю !N! томов (~15 ГБ)...
set /A I=0
for /f %%U in ('type "%TEMP%\gh_parts.txt"') do (
    set /A I+=1
    echo   [!I!/!N!] %%~nxU
    curl -L --retry 5 -C - -o "!DEST!\%%~nxU" "%%U" || goto :err
)

echo [4/4] Распаковываю...
"!DEST!\7zr.exe" x -y -o"!DEST!\Game" "!DEST!\%PREFIX%.*" || goto :err

echo Чищу тома...
del "!DEST!\%PREFIX%.*" >nul 2>&1

powershell -NoProfile -Command "$ws=New-Object -ComObject WScript.Shell;$s=$ws.CreateShortcut([Environment]::GetFolderPath('Desktop')+'\AETHERIA.lnk');$s.TargetPath='!DEST!\Game\Aetheria.exe';$s.WorkingDirectory='!DEST!\Game';$s.Description='AETHERIA: Shards of the Chrono-Seal';$s.Save()"

echo ============================================================================
echo   ГОТОВО! Ярлык AETHERIA на рабочем столе.
echo   Первый запуск компилирует шейдеры 1-2 минуты — это нормально.
echo ============================================================================
pause
exit /b 0

:err
echo.
echo [ОШИБКА] Установка не завершена. Проверьте интернет/место и запустите снова
echo          (докачка продолжится с места обрыва).
pause
exit /b 1
