#!/usr/bin/env bash
# Wait for the co-editing bot to go quiet (no Source/Config mtime CHANGES for
# 8 min), then run the final packaging.
PROJ="C:/Users/tagir/OneDrive/Desktop/Aetheria"
quiet_needed=480
last_newest=0
last_change=$(date +%s)
while true; do
  sleep 60
  newest=$(find Source Config -type f -printf '%T@\n' 2>/dev/null | sort -rn | head -1 | cut -d. -f1)
  if [ "${newest:-0}" -gt "$last_newest" ]; then
    if [ "$last_newest" -gt 0 ]; then
      last_change=$(date +%s)
      echo "$(date +%H:%M:%S) bot activity (mtime bump)"
    else
      echo "$(date +%H:%M:%S) baseline mtime=$newest"
    fi
    last_newest=${newest:-0}
  else
    quiet=$(( $(date +%s) - last_change ))
    echo "$(date +%H:%M:%S) quiet ${quiet}s/${quiet_needed}s"
    if [ "$quiet" -ge "$quiet_needed" ]; then
      echo "$(date +%H:%M:%S) quiet window -> packaging"
      break
    fi
  fi
done
cd "$PROJ" && cmd //c Build_UE5_EXE.bat > Saved/pack_ue5_v6.log 2>&1
echo "EXIT:$?" >> Saved/pack_ue5_v6.log
tail -3 Saved/pack_ue5_v6.log
