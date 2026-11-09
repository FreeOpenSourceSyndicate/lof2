#!/usr/bin/env bash
# Capture TWO frames from the SAME DOSBox session so they share one palette:
# the main menu (before any key) and the town/play screen (after O then 2).
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GAME="$ROOT/game"
OUT="$ROOT/ref/oracle"
WAIT=${1:-34}; POST=${2:-8}
CAPS="$(mktemp -d)"; CONF="$(mktemp)"
cat > "$CONF" <<EOF
[sdl]
fullscreen=false
windowresolution=original
output=surface
[dosbox]
machine=ega
memsize=4
captures=$CAPS
[cpu]
cycles=fixed 3000
core=normal
[autoexec]
mount c $GAME
c:
START.BAT
EOF
export DISPLAY=:99
pkill -f "Xvfb :99" 2>/dev/null; sleep 1
Xvfb :99 -screen 0 1024x768x24 >/dev/null 2>&1 & XVFB_PID=$!; sleep 2
dosbox -conf "$CONF" >"$OUT/dosbox_pair.log" 2>&1 & DB_PID=$!
sleep "$WAIT"
WIN=$(xdotool search --name "DOSBox" | head -1); [ -n "$WIN" ] && xdotool windowactivate "$WIN" 2>/dev/null
xdotool key --clearmodifiers ctrl+F5; sleep 2      # frame 1: main menu
xdotool key --clearmodifiers o; sleep 3
xdotool key --clearmodifiers 2; sleep "$POST"
xdotool key --clearmodifiers ctrl+F5; sleep 2      # frame 2: town/play
IMGS=($(ls -tr "$CAPS"/*.png 2>/dev/null))
echo "captured ${#IMGS[@]} frame(s): ${IMGS[*]}"
[ ${#IMGS[@]} -ge 1 ] && cp "${IMGS[0]}" "$OUT/menu2.png"
[ ${#IMGS[@]} -ge 2 ] && cp "${IMGS[1]}" "$OUT/town2.png"
kill $DB_PID 2>/dev/null; sleep 1; kill $XVFB_PID 2>/dev/null
rm -rf "$CAPS" "$CONF"
