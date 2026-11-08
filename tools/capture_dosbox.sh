#!/usr/bin/env bash
# Capture reference frames from the original game running under DOSBox (headless).
# Usage: capture_dosbox.sh <outdir> <seconds-before-shot> [extra-keys...]
set -u
OUT="${1:-ref}"
WAIT="${2:-25}"
shift 2 || true
EXTRA="$*"
mkdir -p "$OUT"
export DISPLAY=:99
pkill -f "Xvfb :99" 2>/dev/null
sleep 1
Xvfb :99 -screen 0 1024x768x24 >/dev/null 2>&1 &
XVFB_PID=$!
sleep 2
dosbox -conf /tmp/dosbox_lof.conf >"$OUT/dosbox.log" 2>&1 &
DB_PID=$!
sleep "$WAIT"
WIN=$(xdotool search --name "DOSBox" | head -1)
if [ -n "$WIN" ]; then xdotool windowfocus "$WIN" 2>/dev/null; fi
if [ -n "$EXTRA" ]; then xdotool key --clearmodifiers $EXTRA 2>/dev/null; sleep 1; fi
import -window root "$OUT/shot.png" 2>/dev/null
convert "$OUT/shot.png" -crop 640x480+0+0 +repage "$OUT/shot_crop.png" 2>/dev/null
kill $DB_PID 2>/dev/null
sleep 1
kill $XVFB_PID 2>/dev/null
echo "captured $OUT/shot.png"
