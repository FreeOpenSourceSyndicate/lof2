#!/usr/bin/env bash
# golden_test.sh — regression test for the analysis emulator.
#
# Runs the emulator to a fixed frame with a fixed input tail, dumps the EGA
# planes, and compares their hash against a recorded golden value.  This catches
# any change to the CPU, EGA model, DOS/BIOS handlers, or loader that alters the
# rendered output — i.e. it protects the oracle that every drop-in screen is
# verified against.
#
# Run directly, or via `make golden`.  Needs the game data (bootstrap.sh).
#
# To re-record after an INTENDED change:
#   ./tests/golden_test.sh --record
set -u
cd "$(dirname "$0")/.."

FRAMES="${GOLDEN_FRAMES:-60}"
GOLDEN_FILE="tests/golden.txt"
DUMP="$(mktemp -t lofplanes.XXXXXX)"
trap 'rm -f "$DUMP"' EXIT

if [ ! -f game/LOF.EXE ]; then
    echo "golden_test: SKIP (no game data — run ./bootstrap.sh)"
    exit 0
fi
if [ ! -x ./lof-native ]; then
    echo "golden_test: building emulator"; make -s >/dev/null || exit 2
fi

# scenario <name> <extra lof-native args...>
scenario() {
    local name="$1"; shift
    LOF_EDUMP="$DUMP" LOF_CMDLEN=3 LOF_NO_TIMER=1 \
        timeout 300 ./lof-native --dir game --headless --frames "$FRAMES" \
        --shot /dev/null "$@" >/dev/null 2>&1
    printf '%s %s\n' "$name" "$(md5sum "$DUMP" | awk '{print $1}')"
}

# "boot" = the main menu; the rest press a menu hot-key (the menu is
# keyboard-driven) and capture the resulting screen: L=character, O=open,
# A=add, D=dismiss, E=erase.  "temple" opens save slot 1 (O then "1") and
# "play" opens slot 2 (O then "2"); both enter the play loop.  Slot 1 starts
# the party inside the Temple dungeon (dark first-person viewport); slot 2
# starts them on the wilderness map in front of the town (bright viewport).
# The load is long and its timing varies, so the key feed must be adaptive
# (LOF_KEYAUTO) rather than frame-scheduled.  Note the slot is chosen with the
# digit key, not Enter (Enter on the dialog does nothing).
#
# The intro ends in the town's *Inn with no party members*, not the town menu.
# "town" walks out of the Inn into the town (A add char, 2 the 2nd char, C leave
# the add menu, C leave the Inn); "shop" and "temple_bldg" then enter the
# Emporium / Temple.  Each of these screens dwells ~a million instructions, so
# the key feed needs a large gap (LOF_KEYGAP=250) — a small gap delivers the
# next key while the previous screen is still loading, which drops it.
run() {
    scenario boot
    scenario look  LOF_KEYFRAME=10 --keys 0x26
    scenario open  LOF_KEYFRAME=10 --keys 0x18
    scenario add   LOF_KEYFRAME=10 --keys 0x1e
    scenario dism  LOF_KEYFRAME=10 --keys 0x20
    scenario erase LOF_KEYFRAME=10 --keys 0x12
    LOF_KEYAUTO=0x18,0x02 LOF_KEYGAP=8 scenario temple --frames 1500
    LOF_KEYAUTO=0x18,0x03 LOF_KEYGAP=8 scenario play   --frames 1000
    LOF_KEYAUTO=0x1e,0x03,0x2e,0x2e           LOF_KEYGAP=250 scenario town        --frames 1600
    LOF_KEYAUTO=0x1e,0x03,0x2e,0x2e,0x12      LOF_KEYGAP=250 scenario shop        --frames 1600
    LOF_KEYAUTO=0x1e,0x03,0x2e,0x2e,0x14      LOF_KEYGAP=250 scenario temple_bldg --frames 1600
}

if [ "${1:-}" = "--record" ] || [ ! -f "$GOLDEN_FILE" ]; then
    run > "$GOLDEN_FILE"
    echo "golden_test: recorded (frames=$FRAMES):"; cat "$GOLDEN_FILE"
    exit 0
fi

want="$(cat "$GOLDEN_FILE")"
got="$(run)"
if [ "$got" = "$want" ]; then
    echo "golden_test: ok (frames=$FRAMES)"
    exit 0
fi
echo "golden_test: FAIL"
echo "  --- expected ---"; echo "$want"
echo "  --- got      ---"; echo "$got"
echo "  If this change is intended, re-record with: ./tests/golden_test.sh --record"
exit 1
