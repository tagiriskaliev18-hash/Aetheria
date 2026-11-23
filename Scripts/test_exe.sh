#!/usr/bin/env bash
# Full verification pass against the PACKAGED exe (Build/Windows/Aetheria.exe).
# 1) ground test in all 4 worlds, 2) combat test, 3) binding log check,
# 4) staff close-up + world shots. Always kills the game process.
set -u
PROJ="C:/Users/tagir/OneDrive/Desktop/Aetheria"
EXE="$PROJ/Build/Windows/Aetheria.exe"
LOGDIR="$PROJ/Build/Windows/Aetheria/Saved/Logs"

[ -f "$EXE" ] || { echo "NO EXE"; exit 1; }

echo "=== GROUND TESTS ==="
for MAP in World_Bastion World_Ruins World_Sanctum World_Void; do
  LOG="$LOGDIR/Aetheria_ExeGround_${MAP}.log"
  rm -f "$LOG"
  timeout 420 env MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' "$EXE" "$MAP" -game -RenderOffscreen -unattended -nosound -windowed -ResX=1280 -ResY=720 -log -GrokGroundTest -LOG="Aetheria_ExeGround_${MAP}.log" >/dev/null 2>&1
  V=$(grep -a "GroundCheck] VERDICT" "$LOG" 2>/dev/null | tail -1 | sed 's/.*\[GroundCheck\]//')
  echo "$MAP: ${V:-NO VERDICT}"
done

echo "=== COMBAT TEST (Bastion) ==="
LOG="$LOGDIR/Aetheria_ExeCombat.log"
rm -f "$LOG"
timeout 420 env MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' "$EXE" World_Bastion -game -RenderOffscreen -unattended -nosound -windowed -ResX=1280 -ResY=720 -log -GrokCombatTest -LOG="Aetheria_ExeCombat.log" >/dev/null 2>&1
grep -a "CombatTest\]" "$LOG" | sed 's/.*\[CombatTest\]//' | tail -5

echo "=== ANIM BINDING (windowed quick boot) ==="
LOG="$LOGDIR/Aetheria_ExeBind.log"
rm -f "$LOG"
timeout 420 env MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' "$EXE" World_Bastion -game -RenderOffscreen -unattended -nosound -windowed -ResX=1280 -ResY=720 -log -GrokGroundTest -LOG="Aetheria_ExeBind.log" >/dev/null 2>&1
grep -a "Bound mesh\|Bound Paragon\|Late-swapped" "$LOG" | sed 's/.*\[Aetheria\]//' | head -6
echo "DONE"
