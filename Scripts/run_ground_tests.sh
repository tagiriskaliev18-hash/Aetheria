#!/usr/bin/env bash
# Runs the [GroundCheck] automated test in every baked gameplay world.
# Boots each map with -GrokGroundTest -RenderOffscreen, waits for the
# VERDICT line in the log, then always kills the process.
set -u
PROJ="C:/Users/tagir/OneDrive/Desktop/Aetheria"
UE_BIN="C:/Users/tagir/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe"
LOGDIR="$PROJ/Saved/Logs"
TIMEOUT_SECS=150

declare -a WORLDS=(World_Bastion World_Ruins World_Sanctum World_Void)
declare -A RESULTS

for MAP in "${WORLDS[@]}"; do
  LOG="$LOGDIR/Aetheria_Ground_${MAP}.log"
  rm -f "$LOG"
  echo "=== $MAP ==="
  MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' "$UE_BIN" "$PROJ/Aetheria.uproject" "/Game/Maps/AetheriaWorlds/${MAP}" \
    -game -RenderOffscreen -unattended -nosound -windowed -ResX=1280 -ResY=720 \
    -GrokGroundTest -LOG="Aetheria_Ground_${MAP}.log" &
  PID=$!
  VERDICT=""
  ELAPSED=0
  while [ $ELAPSED -lt $TIMEOUT_SECS ]; do
    sleep 5; ELAPSED=$((ELAPSED+5))
    if [ -f "$LOG" ]; then
      V=$(grep -a "GroundCheck] VERDICT" "$LOG" | tail -1)
      if [ -n "$V" ]; then VERDICT="$V"; break; fi
    fi
    # bail out early if the process died
    kill -0 $PID 2>/dev/null || break
  done
  kill $PID 2>/dev/null
  sleep 2
  kill -9 $PID 2>/dev/null
  # UnrealEditor may spawn a child game process; clean by log marker as fallback
  if [ -z "$VERDICT" ] && [ -f "$LOG" ]; then
    VERDICT=$(grep -a "GroundCheck] VERDICT" "$LOG" | tail -1)
  fi
  if [ -z "$VERDICT" ]; then VERDICT="NO VERDICT (timeout or crash)"; fi
  echo "$VERDICT"
  RESULTS[$MAP]="$VERDICT"
done

echo ""
echo "===== SUMMARY ====="
PASS=0
for MAP in "${WORLDS[@]}"; do
  if [[ "${RESULTS[$MAP]}" == *"PASS"* ]]; then PASS=$((PASS+1)); fi
  echo "$MAP: ${RESULTS[$MAP]}"
done
echo "Passed $PASS / ${#WORLDS[@]}"
