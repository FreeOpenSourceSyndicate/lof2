#!/usr/bin/env bash
# Capture a reference frame from the ORIGINAL game running under DOSBox.
#
# The game must be started with START.BAT, not LOF.EXE directly: LOF checks
# that VOR ran first and otherwise prints "Please start LOF with START.BAT" and
# exits.  START.BAT runs VOR (disk copy-protection) and then LOF.
#
# Screenshots use DOSBox's own capture (Ctrl+F5), which saves an exact 320x200
# frame, rather than grabbing the X root window (unreliable here).
#
# Usage: capture_dosbox.sh <outdir> [--wait SEC] [--keys "o 1"] [--post SEC]
#   --wait  seconds to wait before the first key (game boot; default 30)
#   --keys  xdotool key names sent one at a time (e.g. "o 1" = Open, slot 1)
#   --post  seconds to wait after the last key before the shot (default 6)
#
# Requires: dosbox, Xvfb, xdotool (apt install dosbox xvfb xdotool).
set -u

OUT="${1:-ref}"; shift 2>/dev/null || true
WAIT=30; POST=6; KEYS=""
while [ $# -gt 0 ]; do
    case "$1" in
        --wait) WAIT="$2"; shift 2 ;;
        --post) POST="$2"; shift 2 ;;
        --keys) KEYS="$2"; shift 2 ;;
        *) shift ;;
    esac
done

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GAME="$ROOT/game"
[ -d "$GAME" ] || { echo "capture_dosbox: $GAME missing (run ./bootstrap.sh)"; exit 2; }

mkdir -p "$OUT"
CAPS="$(mktemp -d)"
CONF="$(mktemp)"

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
Xvfb :99 -screen 0 1024x768x24 >/dev/null 2>&1 &
XVFB_PID=$!; sleep 2

dosbox -conf "$CONF" >"$OUT/dosbox.log" 2>&1 &
DB_PID=$!
sleep "$WAIT"

WIN=$(xdotool search --name "DOSBox" | head -1)
[ -n "$WIN" ] && xdotool windowactivate "$WIN" 2>/dev/null

for k in $KEYS; do xdotool key --clearmodifiers "$k"; sleep 2; done
sleep "$POST"

xdotool key --clearmodifiers ctrl+F5; sleep 2
SHOT=$(ls "$CAPS"/*.png 2>/dev/null | head -1)
if [ -n "$SHOT" ]; then cp "$SHOT" "$OUT/frame.png"; fi

kill $DB_PID 2>/dev/null; sleep 1
kill $XVFB_PID 2>/dev/null
rm -rf "$CAPS" "$CONF"

if [ -n "$SHOT" ]; then echo "captured $OUT/frame.png"; else echo "capture_dosbox: no frame captured"; exit 1; fi

