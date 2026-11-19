#!/usr/bin/env bash
# golden_test.sh — regression test for the analysis emulator.
#
# Runs the emulator to a fixed frame with a fixed input tail, dumps the EGA
# planes, and compares their hash against a recorded golden value.  This catches
# any change to the CPU, EGA model, DOS/BIOS handlers, or loader that alters the
# rendered output — i.e. it protects the oracle that every drop-in screen is
# verified against.
#
# The scenarios are independent and deterministic, so they run concurrently
# (GOLDEN_JOBS workers, default = nproc); a serial sweep takes ~30 min, the
# parallel one a few minutes.  The hashes are still emitted in the recorded
# order, so tests/golden.txt is unchanged.
#
# Run directly, or via `make golden`.  Needs the game data (bootstrap.sh).
#
# To re-record after an INTENDED change:
#   ./tests/golden_test.sh --record
set -u
cd "$(dirname "$0")/.."

FRAMES="${GOLDEN_FRAMES:-60}"
GOLDEN_FILE="tests/golden.txt"

if [ ! -f game/LOF.EXE ]; then
    echo "golden_test: SKIP (no game data — run ./bootstrap.sh)"
    exit 0
fi
if [ ! -x ./lof-native ]; then
    echo "golden_test: building emulator"; make -s >/dev/null || exit 2
fi

# scenario <name> <extra lof-native args...>
# Each scenario is an independent, deterministic emulator run, so they can run
# concurrently; the hash is written to $OUTDIR/<name>.hash and printed in
# $SCEN_ORDER afterwards, keeping the golden file byte-identical to the order a
# serial run would produce.  A run that times out leaves no dump; report it as
# FAILED so the mismatch names the culprit instead of showing md5-of-nothing.
scenario() {
    local name="$1"; shift
    local dump="$OUTDIR/$name"
    LOF_EDUMP="$dump" LOF_CMDLEN=3 LOF_NO_TIMER=1 \
        timeout "$TIMEOUT" ./lof-native --dir game --headless --frames "$FRAMES" \
        --shot /dev/null "$@" >/dev/null 2>&1
    if [ -s "$dump" ]; then
        echo "$name $(md5sum "$dump" | awk '{print $1}')" > "$OUTDIR/$name.hash"
    else
        echo "$name FAILED(timeout=${TIMEOUT}s)" > "$OUTDIR/$name.hash"
    fi
}

# Scenario table.  SCEN_ORDER is the golden-file order; SCHED_ORDER dispatches
# the same set longest-first, which packs the worker pool far better (the four
# ~270 s scenarios start together instead of queueing behind the quick ones).
SCEN_ORDER="boot look open add dism erase temple play town_entry town shop temple_bldg"
SCHED_ORDER="town shop temple_bldg temple play town_entry boot look open add dism erase"
sc_boot()   { scenario boot; }
sc_look()   { LOF_KEYFRAME=10 scenario look --keys 0x26; }
sc_open()   { LOF_KEYFRAME=10 scenario open --keys 0x18; }
sc_add()    { LOF_KEYFRAME=10 scenario add --keys 0x1e; }
sc_dism()   { LOF_KEYFRAME=10 scenario dism --keys 0x20; }
sc_erase()  { LOF_KEYFRAME=10 scenario erase --keys 0x12; }
sc_temple() { LOF_KEYAUTO=0x18,0x02 LOF_KEYGAP=8 scenario temple --frames 1500; }
sc_play()   { LOF_KEYAUTO=0x18,0x03 LOF_KEYGAP=8 scenario play --frames 1000; }
sc_town_entry() { LOF_KEYAUTO=0x18,0x03,0x50,0x48 LOF_KEYGAP=50 scenario town_entry --frames 600; }
sc_town()   { LOF_KEYAUTO=0x1e,0x03,0x2e,0x2e LOF_KEYGAP=250 scenario town --frames 1600; }
sc_shop()   { LOF_KEYAUTO=0x1e,0x03,0x2e,0x2e,0x12 LOF_KEYGAP=250 scenario shop --frames 1600; }
sc_temple_bldg() { LOF_KEYAUTO=0x1e,0x03,0x2e,0x2e,0x14 LOF_KEYGAP=250 scenario temple_bldg --frames 1600; }

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
    OUTDIR="$(mktemp -d -t lofgold.XXXXXX)"
    local jobs="${GOLDEN_JOBS:-$(nproc 2>/dev/null || echo 4)}"
    # Workers contend for memory bandwidth, so a scenario runs slower in the
    # pool than alone (the ~250 s heavy ones take ~450 s at 4-way).  Scale the
    # per-run ceiling with the pool width; it is only a hang guard.
    TIMEOUT="${GOLDEN_TIMEOUT:-$((300 * jobs))}"
    export OUTDIR TIMEOUT FRAMES
    local running=0
    for name in $SCHED_ORDER; do
        "sc_$name" &
        running=$((running + 1))
        if [ "$running" -ge "$jobs" ]; then
            # -n returns the *job's* exit status (non-zero if a scenario
            # failed), so never let that fall through to a full `wait`.
            wait -n 2>/dev/null || true
            running=$((running - 1))
        fi
    done
    wait
    local name
    for name in $SCEN_ORDER; do cat "$OUTDIR/$name.hash"; done
    rm -rf "$OUTDIR"
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
