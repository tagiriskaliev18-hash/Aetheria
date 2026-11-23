# One-map-at-a-time extract. Never open ParagonSample or Daylight.
# Run from an already-empty session after UnrealEditor is closed.
$ErrorActionPreference = "Stop"
$ue = "C:\Users\tagir\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$proj = "C:\Users\tagir\OneDrive\Desktop\Aetheria\Aetheria.uproject"
$scripts = "C:\Users\tagir\OneDrive\Desktop\Aetheria\Scripts\editor_scripts"
$logdir = "C:\Users\tagir\OneDrive\Desktop\Aetheria\Saved\Logs"

function Assert-NoEditor {
    $p = Get-Process UnrealEditor, UnrealEditor-Cmd, UnrealEditor-Win64-Development -EA SilentlyContinue
    if ($p) { throw "Editor still running: $($p.Id -join ',')" }
}

function Wait-FreeRam([int]$NeedMB = 5000) {
    $os = Get-CimInstance Win32_OperatingSystem
    $free = [int]($os.FreePhysicalMemory / 1KB)
    "FreeMB=$free need>=$NeedMB"
    if ($free -lt $NeedMB) { throw "Not enough free RAM ($free MB)" }
}

function Run-Python([string]$Map, [string]$Script, [string]$LogName) {
    Assert-NoEditor
    Wait-FreeRam 4500
    $log = Join-Path $logdir $LogName
    $py = Join-Path $scripts $Script
    $args = @($proj)
    if ($Map) { $args += $Map }
    $args += @("-ExecutePythonScript=$py", "-unattended", "-nosplash", "-ABSLOG=$log")
    Write-Host "LAUNCH $($args -join ' ')"
    $p = Start-Process -FilePath $ue -ArgumentList $args -PassThru
    $ok = $p.WaitForExit(420000)
    if (-not $ok) {
        Stop-Process -Id $p.Id -Force -EA SilentlyContinue
        throw "Timeout $LogName"
    }
    Write-Host "EXIT $($p.ExitCode) log=$log"
    # let the OS release RAM
    Start-Sleep -Seconds 8
}

Assert-NoEditor
# 1) Duplicate packages against the tiny persistent map only
Run-Python "/Game/Maps/AetheriaWorld" "duplicate_demo_maps.py" "dup_layouts.log"
# 2) Inventory each layout alone, lightest first
Run-Python "/Game/Maps/AetheriaLayouts/Layout_GothicCourtyard" "inventory_loaded_map.py" "inv_gothic.log"
Run-Python "/Game/Maps/AetheriaLayouts/Layout_TreasureSanctum" "inventory_loaded_map.py" "inv_treasures.log"
Run-Python "/Game/Maps/AetheriaLayouts/Layout_MedievalDungeon" "inventory_loaded_map.py" "inv_dungeon.log"
Write-Host "DONE extract"
