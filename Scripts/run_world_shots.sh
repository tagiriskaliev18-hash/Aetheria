#!/usr/bin/env bash
# Boots each world map with -GrokWorldShot and waits for the
# "[Aetheria] World shot" log line (screenshot saved) before moving on.
# Engine init + shader compile can take minutes per fresh boot — cap 10 min.
set -u
PROJ="C:/Users/tagir/OneDrive/Desktop/Aetheria"
UE_BIN="C:/Users/tagir/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe"
LOGDIR="$PROJ/Saved/Logs"
CAP_ITERS=120   # 120 * 5s = 600s per map

declare -a WORLDS=("${@:-World_Bastion World_Ruins World_Sanctum World_Void}")

for MAP in "${WORLDS[@]}"; do
  TAG="${MAP#World_}"
  LOG="$LOGDIR/Aetheria_QA_${MAP}.log"
  rm -f "$LOG"
  echo "=== $MAP ==="
  MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' "$UE_BIN" "$PROJ/Aetheria.uproject" "/Game/Maps/AetheriaWorlds/$MAP" \
    -game -RenderOffscreen -unattended -nosound -windowed -ResX=1280 -ResY=720 \
    -GrokWorldShot -LOG="Aetheria_QA_${MAP}.log" > /dev/null 2>&1 &
  PID=$!
  DONE=""
  for i in $(seq 1 $CAP_ITERS); do
    sleep 5
    if [ -f "$LOG" ] && grep -aq "World shot" "$LOG" 2>/dev/null; then
      grep -a "World shot" "$LOG" | tail -1
      sleep 3   # let the PNG flush finish before killing (raced at 16:29-16:31, shots lost)
      DONE=1; break
    fi
  done
  [ -z "$DONE" ] && echo "TIMEOUT after $((CAP_ITERS*5))s"
  # kill both wrapper and child game process
  kill $PID 2>/dev/null
  taskkill //IM UnrealEditor.exe //F >/dev/null 2>&1
  taskkill //IM UnrealEditor-Cmd.exe //F >/dev/null 2>&1
  sleep 3
done
echo "===== DONE ====="
ls -t "$PROJ/Saved/GrokShots"/worldshot_*.png | head -4
