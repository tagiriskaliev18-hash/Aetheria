"""
Automated UE 5.8.4 Standalone Package, Server Launchers, Single-File Setup.exe & .zip Transfer Kit Builder.
Stages runtime Non-UFS folders cleanly, runs RunUAT BuildCookRun, creates Listen/Dedicated server helpers,
wraps Build/Windows/* into self-extracting Release/AETHERIA_Setup.exe + transfer .zip archives.
"""

from pathlib import Path
import io
import shutil
import struct
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parent.parent
UE_RUNUAT = Path(r"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat")
SEVEN_ZIP = Path(r"C:\Program Files\7-Zip\7z.exe")
REDIST_X64 = Path(r"C:\Program Files\Epic Games\UE_5.8\Engine\Extras\Redist\en-us\vc_redist.x64.exe")
STUB_EXE = ROOT / "Installer" / "AetheriaSetupStub.exe"
ICON_PATH = ROOT / "Installer" / "Aetheria.ico"
BUILD_ROOT = ROOT / "Build"
STAGED_WIN = BUILD_ROOT / "Windows"
RELEASE_DIR = ROOT / "Release"
PAYLOAD_MAGIC = 0x4145544845524941  # "AETHERIA" little-endian


def run_checked(cmd, cwd=ROOT, check=True):
    print(f"\n>>> RUN: {' '.join(str(c) for c in cmd)}")
    proc = subprocess.run([str(c) for c in cmd], cwd=str(cwd))
    if check and proc.returncode != 0:
        raise RuntimeError(f"Command failed with exit code {proc.returncode}: {cmd}")
    return proc.returncode


def ensure_default_game_ini():
    data_src = ROOT / "Data" / "LootTables.json"
    data_dst = ROOT / "Content" / "Data" / "LootTables.json"
    if data_src.exists():
        data_dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(data_src, data_dst)

    ini_path = ROOT / "Config" / "DefaultGame.ini"
    lines = ini_path.read_text(encoding="utf-8-sig").splitlines()
    new_lines = []
    has_data_stage = False
    for line in lines:
        s = line.strip()
        if s in (
            '+DirectoriesToAlwaysStageAsNonUFS=(Path="RawAssets")',
            '+DirectoriesToAlwaysStageAsNonUFS=(Path="Design")',
        ):
            continue
        if s == '+DirectoriesToAlwaysStageAsNonUFS=(Path="Data")':
            has_data_stage = True
        new_lines.append(line)

    if not has_data_stage:
        new_lines.append('+DirectoriesToAlwaysStageAsNonUFS=(Path="Data")')

    ini_path.write_text("\n".join(new_lines) + "\n", encoding="utf-8")
    print("[OK] Config/DefaultGame.ini NonUFS staging normalized to Content/Data")


def copy_tree_clean(src: Path, dst: Path):
    if not src.exists():
        print(f"[WARN] Optional folder missing, skipped: {src}")
        return
    shutil.copytree(src, dst, dirs_exist_ok=True)
    print(f"[OK] Staged {src.relative_to(ROOT)} -> {dst.relative_to(ROOT)}")


def write_batch_utf8(path: Path, body: str):
    # Normalize CRLF line endings cleanly for cmd.exe
    text = body.replace("\r\n", "\n").replace("\n", "\r\n")
    path.write_bytes(text.encode("utf-8"))


def sync_newer_spellplates():
    tex_dir = ROOT / "RawAssets" / "Textures"
    grim_dir = ROOT / "RawAssets" / "Grimoire"
    if not tex_dir.exists() or not grim_dir.exists():
        return
    for src in tex_dir.glob("SpellPlate_*.png"):
        dst = grim_dir / src.name
        if not dst.exists() or src.stat().st_mtime > dst.stat().st_mtime:
            shutil.copy2(src, dst)
            print(f"[SYNC] Updated {dst.relative_to(ROOT)} from newer {src.relative_to(ROOT)}")


def stage_launchers_and_runtime():
    proj_stage = STAGED_WIN / "Aetheria"
    if not proj_stage.exists():
        raise RuntimeError(f"Missing packaged project folder: {proj_stage}")

    sync_newer_spellplates()

    # Copy only runtime Non-UFS folders needed directly by HUD/materials/JSON (~30 MB, zero raw 2K/.blend bloat)
    copy_tree_clean(ROOT / "RawAssets" / "Fonts", proj_stage / "RawAssets" / "Fonts")
    copy_tree_clean(ROOT / "RawAssets" / "UI", proj_stage / "RawAssets" / "UI")
    copy_tree_clean(ROOT / "RawAssets" / "Grimoire", proj_stage / "RawAssets" / "Grimoire")
    copy_tree_clean(ROOT / "Design", proj_stage / "Design")

    loot_src = ROOT / "Data" / "LootTables.json"
    if loot_src.exists():
        for rel in (Path("Data/LootTables.json"), Path("Content/Data/LootTables.json")):
            dst = proj_stage / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(loot_src, dst)

    if ICON_PATH.exists():
        shutil.copy2(ICON_PATH, STAGED_WIN / "Application.ico")

    if REDIST_X64.exists():
        redist_dst = STAGED_WIN / "Redist" / "vc_redist.x64.exe"
        redist_dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(REDIST_X64, redist_dst)

    # 1. Play client launcher
    write_batch_utf8(
        STAGED_WIN / "Play_AETHERIA.bat",
        """@echo off
chcp 65001 >nul
cd /d "%~dp0"
start "" "%~dp0Aetheria.exe" /Engine/Maps/Entry -WindowedFullscreen
exit /b 0
""",
    )

    # 2. Listen Server Host + Play immediately (0ms ping for host)
    write_batch_utf8(
        STAGED_WIN / "Host_Coop_Server_7777.bat",
        """@echo off
chcp 65001 >nul
cd /d "%~dp0"
echo ============================================================================
echo   AETHERIA - Co-op Хост Сервер (UDP порт 7777, до 6 магов)
echo   Друзья вводят ваш IP:7777 в Лобби и нажимают "ВОЙТИ IP".
echo   Чтобы узнать свой IP адрес, запустите Show_My_Server_IPs.bat
echo ============================================================================
start "" "%~dp0Aetheria.exe" /Engine/Maps/Entry?listen?Loc=0?Bots=0?Pacing=1 -Port=7777 -WindowedFullscreen -log=CoopHost_7777.log
exit /b 0
""",
    )

    # 3. Dedicated headless server (no GPU required)
    write_batch_utf8(
        STAGED_WIN / "Start_Dedicated_Server_7777.bat",
        """@echo off
chcp 65001 >nul
title AETHERIA Dedicated Server (Port UDP 7777)
cd /d "%~dp0"
echo ============================================================================
echo   AETHERIA DEDICATED SERVER (UDP порт 7777 · без окна 3D-рендера)
echo   Подключайтесь на этот же ПК через 127.0.0.1:7777
echo   Друзья подключаются по вашему IP:7777 (Show_My_Server_IPs.bat)
echo   Не закрывайте это окно, пока идёт игра!
echo ============================================================================
"%~dp0Aetheria\\Binaries\\Win64\\Aetheria.exe" /Engine/Maps/Entry?Loc=0?Bots=0?Pacing=1 -server -Port=7777 -log -LOG=DedicatedServer_7777.log
pause
""",
    )

    # 4. Self-elevating Windows Firewall opener for UDP 7777 & Game .exe
    write_batch_utf8(
        STAGED_WIN / "Open_Firewall_Port_7777_Admin.bat",
        """@echo off
chcp 65001 >nul
net session >nul 2>&1
if %errorlevel% neq 0 (
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs -Wait"
    exit /b %errorlevel%
)
cd /d "%~dp0"
echo Открываем порт UDP 7777 и разрешаем Aetheria.exe в Брандмауэре Windows...
netsh advfirewall firewall delete rule name="AETHERIA_UDP_7777" >nul 2>&1
netsh advfirewall firewall delete rule name="AETHERIA_GameExe" >nul 2>&1
netsh advfirewall firewall add rule name="AETHERIA_UDP_7777" dir=in action=allow protocol=UDP localport=7777 profile=any
netsh advfirewall firewall add rule name="AETHERIA_UDP_7777" dir=out action=allow protocol=UDP localport=7777 profile=any
netsh advfirewall firewall add rule name="AETHERIA_GameExe" dir=in action=allow program="%~dp0Aetheria\\Binaries\\Win64\\Aetheria.exe" enable=yes profile=any
echo Готово! Порт UDP 7777 открыт для локальной сети и VPN (Radmin / Tailscale / ZeroTier).
timeout /t 2 >nul
exit /b 0
""",
    )

    # 5. Show all IPv4 addresses + port 7777 listener status
    write_batch_utf8(
        STAGED_WIN / "Show_My_Server_IPs.bat",
        """@echo off
chcp 65001 >nul
echo ============================================================================
echo   ВАШИ IP-АДРЕСА ДЛЯ ПОДКЛЮЧЕНИЯ ДРУЗЕЙ К СЕРВЕРУ AETHERIA (порт 7777)
echo ============================================================================
powershell -NoProfile -Command "Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.IPAddress -ne '127.0.0.1' -and $_.IPAddress -notlike '169.254.*' } | Sort-Object InterfaceMetric | ForEach-Object { $tag = if ($_.IPAddress -like '26.*') {'[Radmin VPN]'} elseif ($_.IPAddress -like '100.*') {'[Tailscale]'} elseif ($_.IPAddress -like '192.168.*' -or $_.IPAddress -like '10.*') {'[LAN / Wi-Fi / ZeroTier]'} else {'[Net]'}; Write-Host ('  {0,-24} {1}:7777' -f $tag, $_.IPAddress) }; Write-Host ''; $ep = Get-NetUDPEndpoint -LocalPort 7777 -ErrorAction SilentlyContinue; if ($ep) { Write-Host '[OK] Сервер СЕЙЧАС слушает порт UDP 7777!' -ForegroundColor Green } else { Write-Host '[INFO] Порт 7777 пока свободен — запустите Host_Coop_Server_7777.bat или Start_Dedicated_Server_7777.bat' -ForegroundColor Yellow }"
echo.
pause
""",
    )

    # 5b. One-click shortcuts + firewall setup for Portable ZIP users
    write_batch_utf8(
        STAGED_WIN / "Setup_Shortcuts_And_Firewall.bat",
        """@echo off
chcp 65001 >nul
cd /d "%~dp0"
echo Создаём ярлыки AETHERIA и Выделенного Сервера на Рабочем столе...
powershell -NoProfile -Command "$ws = New-Object -ComObject WScript.Shell; $desk = [Environment]::GetFolderPath('Desktop'); $ico = Join-Path '%~dp0' 'Application.ico'; if (!(Test-Path $ico)) { $ico = Join-Path '%~dp0' 'Aetheria.exe' }; $s1 = $ws.CreateShortcut((Join-Path $desk 'AETHERIA.lnk')); $s1.TargetPath = Join-Path '%~dp0' 'Aetheria.exe'; $s1.Arguments = '-WindowedFullscreen'; $s1.WorkingDirectory = '%~dp0'.TrimEnd('\'); $s1.IconLocation = $ico; $s1.Save(); $s2 = $ws.CreateShortcut((Join-Path $desk 'AETHERIA Dedicated Server (Port 7777).lnk')); $s2.TargetPath = Join-Path '%~dp0' 'Start_Dedicated_Server_7777.bat'; $s2.WorkingDirectory = '%~dp0'.TrimEnd('\'); $s2.IconLocation = $ico; $s2.Save(); Write-Host '[OK] Ярлыки созданы на Рабочем столе.'"
call "%~dp0Open_Firewall_Port_7777_Admin.bat"
pause
""",
    )

    # 6. Clear Russian guide
    guide_text = """================================================================================
  AETHERIA: ОСКОЛКИ ХРОНО-ПЕЧАТИ — УСТАНОВКА, СЕРВЕР И CO-OP ДЛЯ 1–6 ДРУЗЕЙ
================================================================================

1. БЫСТРАЯ УСТАНОВКА НА СВОЙ ПК
   - Распакуйте ZIP-архив и запустите AETHERIA_Setup.exe.
   - Выберите папку (по умолчанию C:\\Games\\AETHERIA), оставьте галочки на ярлыках
     и открытии порта UDP 7777, нажмите «УСТАНОВИТЬ ИГРУ И СЕРВЕР».
   - При первом запуске на абсолютно чистом ПК, если возникнет ошибка VCRUNTIME/DLL,
     установите Redist\\vc_redist.x64.exe (или отметьте галочку VC++ в установщике).

2. ДВА СПОСОБА ЗАПУСТИТЬ СЕРВЕР НА СВОЁМ ПК
   Способ А — ИГРАТЬ И ХОСТИТЬ В ОДНОМ ОКНЕ (проще всего):
   - Вариант 1: Запустите Aetheria.exe -> в Лобби нажмите «CO-OP ХОСТ».
   - Вариант 2: Запустите Host_Coop_Server_7777.bat (сразу запускает ?listen на UDP 7777).

   Способ Б — ОТДЕЛЬНЫЙ ВЫДЕЛЕННЫЙ СЕРВЕР (Dedicated Server без нагрузки на видеокарту):
   - Запустите Start_Dedicated_Server_7777.bat (или ярлык на Рабочем столе).
   - Откроется консольное окно сервера на порту UDP 7777 (не закрывайте его).
   - Вы сами на том же ПК открываете Play_AETHERIA.bat, в поле адреса внизу Лобби
     оставляете 127.0.0.1:7777 и нажимаете «ВОЙТИ IP».

3. КАК ПРИГЛАСИТЬ ДРУЗЕЙ (ПОДКЛЮЧЕНИЕ ПО СЕТИ)
   - Передайте друзьям тот же файл AETHERIA_Setup.exe (установщик сам всё распакует).
   - Простейший способ без настройки роутера: создайте общую сеть в Radmin VPN или
     Tailscale с друзьями, запустите Show_My_Server_IPs.bat и скопируйте им строку
     вида 26.x.x.x:7777 (или внешний IP:7777, если на роутере проброшен порт UDP 7777).
   - Друзья вводят ваш IP:7777 в поле внизу Лобби и нажимают «ВОЙТИ IP».

4. ОСНОВНОЕ УПРАВЛЕНИЕ В БОЮ
   ПКМ                — Движение к курсору
   Q / W / E          — Призвать сферы (Ignis / Tempest / Graviton)
   R                  — Зарядить/Синтезировать заклинание (в классическом режиме)
   D / F + ЛКМ        — Применить заряженное заклинание (в Мгновенном режиме — сразу)
   СКМ или Alt+R      — Self-Cast (применение на себя)
   Space              — Рывок (Glide Dash со следом)
   H или 1            — Фляга исцеления (пополняется каждые 4 убийства)
   Tab                — Гримуар заклинаний
   M                  — Атлас 10 миров
   C                  — Редактор облика персонажа
   G                  — Кузня Эссенции (прокачка стихий, HP и маны)
   F6                 — Переключить Friendly Fire (30% / 0%)
   Esc                — Настройки / Лобби / Выход
================================================================================
"""
    (STAGED_WIN / "ИНСТРУКЦИЯ_ЗАПУСК_СЕРВЕРА_И_ИГРЫ.txt").write_bytes(guide_text.encode("utf-8-sig"))
    print("[OK] Staged launchers & guide into Build/Windows")


class _SliceFile(io.RawIOBase):
    def __init__(self, base_file, offset: int, length: int):
        self._f = base_file
        self._offset = offset
        self._length = length
        self._pos = 0

    def readable(self):
        return True

    def seekable(self):
        return True

    def tell(self):
        return self._pos

    def seek(self, offset, whence=io.SEEK_SET):
        if whence == io.SEEK_SET:
            target = offset
        elif whence == io.SEEK_CUR:
            target = self._pos + offset
        elif whence == io.SEEK_END:
            target = self._length + offset
        else:
            raise ValueError("Unsupported whence")
        self._pos = max(0, min(self._length, target))
        return self._pos

    def read(self, size=-1):
        rem = self._length - self._pos
        if rem <= 0:
            return b""
        to_read = rem if size is None or size < 0 else min(size, rem)
        self._f.seek(self._offset + self._pos)
        data = self._f.read(to_read)
        self._pos += len(data)
        return data

    def readinto(self, b):
        data = self.read(len(b))
        n = len(data)
        b[:n] = data
        return n


def verify_setup_dual(setup_out: Path):
    total_sz = setup_out.stat().st_size
    with setup_out.open("rb") as f:
        f.seek(total_sz - 16)
        zip_offset, magic = struct.unpack("<QQ", f.read(16))
        if magic != PAYLOAD_MAGIC or zip_offset <= 0 or zip_offset >= total_sz - 16:
            raise RuntimeError(f"Invalid footer in {setup_out}: offset={zip_offset}, magic={hex(magic)}")

        zip_len = (total_sz - 16) - zip_offset
        with zipfile.ZipFile(_SliceFile(f, zip_offset, zip_len), "r") as zf:
            names_lower = {n.replace("\\", "/").lower() for n in zf.namelist()}
            required = (
                "aetheria.exe",
                "play_aetheria.bat",
                "start_dedicated_server_7777.bat",
                "aetheria/rawassets/fonts/aetheriadisplay-bold.ttf",
                "aetheria/data/loottables.json",
            )
            missing = [req for req in required if req not in names_lower]
            if missing:
                raise RuntimeError(f"Missing required entries in setup payload: {missing}")
            bad_crc = zf.testzip()
            if bad_crc is not None:
                raise RuntimeError(f"CRC-32 corruption detected on entry: {bad_crc}")
            print(f"[OK] Python PE-overlay ZIP CRC-32 verified ({len(names_lower)} entries, 0 errors)")

    ps_script = (
        f"$asm = [System.Reflection.Assembly]::Load([System.IO.File]::ReadAllBytes('{STUB_EXE}')); "
        f"$pr = $asm.GetType('AetheriaSetup.PayloadReader'); "
        f"$args = @([string]'{setup_out}', [string]''); "
        f"$rc = $pr.GetMethod('VerifyPayload').Invoke($null, $args); "
        f"Write-Host $args[1]; exit [int]$rc"
    )
    clr = subprocess.run(["powershell", "-NoProfile", "-Command", ps_script], capture_output=True, text=True)
    print(f"[CLR VERIFY] rc={clr.returncode} out={clr.stdout.strip()} err={clr.stderr.strip()}")
    if clr.returncode != 0 or "VERIFY_OK" not in clr.stdout:
        raise RuntimeError("In-process CLR PayloadReader.VerifyPayload failed!")


def build_setup_and_zips():
    if not STUB_EXE.exists():
        raise RuntimeError(f"Missing Setup stub: {STUB_EXE}")
    if not SEVEN_ZIP.exists():
        raise RuntimeError(f"Missing 7-Zip: {SEVEN_ZIP}")

    RELEASE_DIR.mkdir(parents=True, exist_ok=True)
    payload_zip = ROOT / "Installer" / "Payload.zip"
    if payload_zip.exists():
        payload_zip.unlink()

    run_checked([SEVEN_ZIP, "a", "-tzip", "-mx=5", "-mmt=on", payload_zip, f"{STAGED_WIN}\\*"])

    setup_out = RELEASE_DIR / "AETHERIA_Setup.exe"
    if setup_out.exists():
        setup_out.unlink()

    stub_bytes = STUB_EXE.read_bytes()
    zip_offset = len(stub_bytes)
    with setup_out.open("wb") as out_f:
        out_f.write(stub_bytes)
        with payload_zip.open("rb") as pf:
            shutil.copyfileobj(pf, out_f, 1024 * 1024)
        out_f.write(struct.pack("<QQ", zip_offset, PAYLOAD_MAGIC))

    portable_zip = RELEASE_DIR / "AETHERIA_Portable_Server_And_Game.zip"
    if portable_zip.exists():
        portable_zip.unlink()
    shutil.move(str(payload_zip), str(portable_zip))
    print(f"[OK] Built {setup_out} ({setup_out.stat().st_size / (1024*1024):.1f} MB)")

    verify_setup_dual(setup_out)

    # Transfer kit ZIP (AETHERIA_Setup.exe + Readme)
    kit_zip = RELEASE_DIR / "AETHERIA_Full_Home_And_Server_Kit.zip"
    if kit_zip.exists():
        kit_zip.unlink()
    guide_copy = RELEASE_DIR / "ИНСТРУКЦИЯ_ЗАПУСК_СЕРВЕРА_И_ИГРЫ.txt"
    shutil.copy2(STAGED_WIN / "ИНСТРУКЦИЯ_ЗАПУСК_СЕРВЕРА_И_ИГРЫ.txt", guide_copy)
    run_checked([SEVEN_ZIP, "a", "-tzip", "-mx=1", "-mmt=on", kit_zip, setup_out, guide_copy])

    # Lightweight source & assets backup ZIP (~10-15 MB)
    src_zip = RELEASE_DIR / "AETHERIA_SourceCode_Backup.zip"
    if src_zip.exists():
        src_zip.unlink()
    backup_targets = [
        ROOT / "Aetheria.uproject",
        ROOT / "Source",
        ROOT / "Config",
        ROOT / "Scripts",
        ROOT / "Data",
        ROOT / "Design",
        ROOT / "Installer",
        ROOT / "RawAssets" / "Fonts",
        ROOT / "RawAssets" / "UI",
        ROOT / "RawAssets" / "Grimoire",
        ROOT / "Play_Trial_UE5.bat",
        ROOT / "Build_UE5_EXE.bat",
    ]
    existing_targets = [p for p in backup_targets if p.exists()]
    run_checked([SEVEN_ZIP, "a", "-tzip", "-mx=5", "-mmt=on", src_zip] + existing_targets)

    print("\n=== RELEASE ARTIFACTS READY ===")
    for item in (kit_zip, setup_out, portable_zip, src_zip):
        print(f"  {item.name}: {item.stat().st_size / (1024*1024):.2f} MB")


def compile_stub_exe():
    csc = Path(r"C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe")
    cs_src = ROOT / "Installer" / "AetheriaSetupStub.cs"
    if not csc.exists() or not cs_src.exists():
        print("[WARN] csc.exe or AetheriaSetupStub.cs missing; keeping existing stub.")
        return
    cmd = [
        csc,
        "/nologo",
        "/optimize+",
        "/target:winexe",
        f"/out:{STUB_EXE}",
        "/reference:System.dll,System.Drawing.dll,System.Windows.Forms.dll,System.IO.Compression.dll,System.IO.Compression.FileSystem.dll",
    ]
    ico = ROOT / "Installer" / "Aetheria.ico"
    if not ico.exists() and (Path(r"C:\Games\AETHERIA\Application.ico")).exists():
        ico = Path(r"C:\Games\AETHERIA\Application.ico")
    if ico.exists():
        cmd.append(f"/win32icon:{ico}")
    cmd.append(str(cs_src))
    run_checked(cmd)
    print(f"[OK] Compiled installer stub: {STUB_EXE}")


def repack_installed(installed_dir: Path, setup_out: Path | None = None):
    if not installed_dir.exists():
        raise RuntimeError(f"Installed runtime directory not found: {installed_dir}")
    compile_stub_exe()
    stub_sync = installed_dir / "Aetheria" / "Installer" / "AetheriaSetupStub.exe"
    if stub_sync.parent.exists():
        shutil.copy2(STUB_EXE, stub_sync)

    target_exe = setup_out or (ROOT / "AETHERIA_Setup.exe")
    if target_exe.exists():
        bak_exe = target_exe.with_name(f"{target_exe.name}.bak-20261116")
        if bak_exe.exists():
            bak_exe.unlink()
        shutil.move(str(target_exe), str(bak_exe))
        print(f"[OK] Backed up previous installer to {bak_exe.name}")

    payload_zip = ROOT / "Installer" / "Payload_repack.zip"
    if payload_zip.exists():
        payload_zip.unlink()

    skip_dirs = {"logs", "crashes", "telemetry"}
    file_count = 0
    with zipfile.ZipFile(payload_zip, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=5, allowZip64=True) as zf:
        for p in sorted(installed_dir.rglob("*")):
            if not p.is_file():
                continue
            rel = p.relative_to(installed_dir).as_posix()
            rel_low = rel.lower()
            if ".bak" in rel_low or rel_low.endswith("aetheria_setup.exe"):
                continue
            parts_low = [part.lower() for part in p.relative_to(installed_dir).parts]
            if any(sd in parts_low for sd in skip_dirs):
                continue
            zf.write(p, arcname=rel)
            file_count += 1
    print(f"[OK] Packed {file_count} runtime files into {payload_zip.name} ({payload_zip.stat().st_size / (1024*1024):.1f} MB)")

    stub_bytes = STUB_EXE.read_bytes()
    zip_offset = len(stub_bytes)
    with target_exe.open("wb") as out_f:
        out_f.write(stub_bytes)
        with payload_zip.open("rb") as pf:
            shutil.copyfileobj(pf, out_f, 1024 * 1024)
        out_f.write(struct.pack("<QQ", zip_offset, PAYLOAD_MAGIC))
    payload_zip.unlink()
    print(f"[OK] Built {target_exe} ({target_exe.stat().st_size / (1024*1024):.1f} MB)")
    verify_setup_dual(target_exe)


def main():
    if "--repack-installed" in sys.argv:
        idx = sys.argv.index("--repack-installed")
        inst = Path(sys.argv[idx + 1]) if idx + 1 < len(sys.argv) else Path(r"C:\Games\AETHERIA")
        out_arg = Path(sys.argv[sys.argv.index("--out") + 1]) if "--out" in sys.argv else None
        repack_installed(inst, out_arg)
        return

    reuse_cooked = "--reuse-cooked" in sys.argv
    ensure_default_game_ini()

    if STAGED_WIN.exists():
        shutil.rmtree(STAGED_WIN, ignore_errors=True)
    BUILD_ROOT.mkdir(parents=True, exist_ok=True)

    uat_args = [
        UE_RUNUAT,
        "BuildCookRun",
        f"-project={ROOT / 'Aetheria.uproject'}",
        "-noP4",
        "-platform=Win64",
        "-clientconfig=Development",
    ]
    if reuse_cooked and (ROOT / "Saved" / "Cooked" / "Windows" / "ue.projectstore").exists():
        print("[INFO] Reusing completed 4006-package Zen cook (-skipcook -skipbuild)...")
        uat_args += ["-skipbuild", "-skipcook"]
    else:
        uat_args += ["-cook", "-IgnoreCookErrors", "-map=/Engine/Maps/Entry", "-build"]

    uat_args += [
        "-stage",
        "-pak",
        "-iostore",
        "-archive",
        f"-archivedirectory={BUILD_ROOT}",
        "-utf8output",
    ]
    run_checked(uat_args)

    stage_launchers_and_runtime()
    build_setup_and_zips()

    desktop_setup = Path(r"C:\Users\user\Desktop\AETHERIA_Setup.exe")
    src_setup = RELEASE_DIR / "AETHERIA_Setup.exe"
    if src_setup.exists():
        shutil.copy2(src_setup, desktop_setup)
        print(f"[OK] Copied installer to Desktop: {desktop_setup}")


if __name__ == "__main__":
    main()
