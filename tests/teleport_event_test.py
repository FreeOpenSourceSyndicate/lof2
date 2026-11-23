#!/usr/bin/env python3
"""Type-11 teleport event, end to end.

The handler (0x20CD7..0x20CE4) writes the record's +0xe4 straight into the
party-cell global [0x3824] with no passability check and no facing change, so
walking onto a type-11 event teleports the party to that cell on the same map.

DNG17 (location 17) places SPEC7[13] -- the Air Elemental, type 11, +0xe4=629 --
at (20,18).  Standing at (19,18) facing east and stepping forward fires it; the
party must land on cell 629 = (17,17) with the facing unchanged.  A no-step
control asserts the party stays put.
"""
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOF = os.path.join(ROOT, "lof")
SAVE = os.path.join(ROOT, "game", "GAMES", "GAME2")

START_CELL = 18 * 36 + 19          # (19,18)
FACING_EAST = 1
FORWARD = "0x48"                   # up-arrow: step in the facing direction

PARTY_CELL = 0x9A0
PARTY_FACING = 0x9A6
LOC_ONES = 0x9A2
LOC_TENS = 0x9A4
CHECKSUM_AT = 0x2E3A

PROMPT = "An Air Elemental appears"
TELEPORT = "(teleport -> cell 629)"
FINAL = "walk: x=17 y=17 facing=1"


def checksum(data):
    acc = 1
    for i, byte in enumerate(data):
        value = byte if byte else 1
        acc = (acc + value) & 0xFFFFFFFF if i % 2 == 0 else acc ^ value
    return acc & 0xFFFFFFFF


def make_save(path):
    data = bytearray(open(SAVE, "rb").read())
    struct.pack_into("<H", data, PARTY_CELL, START_CELL)
    data[PARTY_FACING] = FACING_EAST
    data[LOC_ONES] = 7      # location 17 -> DNG17, SYST/SPEC7
    data[LOC_TENS] = 1
    struct.pack_into("<I", data, CHECKSUM_AT, checksum(data[:CHECKSUM_AT]))
    with open(path, "wb") as f:
        f.write(data)


def run(save, keys):
    p = subprocess.run(
        [LOF, "--dir", os.path.join(ROOT, "game"), "--walk",
         "--save", save, "--walk-keys", keys],
        capture_output=True, text=True, cwd=ROOT,
    )
    if p.returncode != 0:
        sys.exit("teleport_event_test: lof --walk failed: %s" % p.stderr.strip())
    return p.stdout


def main():
    if not os.path.exists(LOF):
        sys.exit("teleport_event_test: build the port first (make port)")
    if not os.path.exists(SAVE):
        sys.exit("teleport_event_test: missing fixture %s" % SAVE)

    workdir = tempfile.mkdtemp(prefix="lof-tp-")
    try:
        save = os.path.join(workdir, "TP.SAV")
        make_save(save)

        # No step: the party stays at the spawn cell, no teleport.
        out0 = run(save, "")
        if TELEPORT in out0:
            sys.exit("teleport_event_test: teleported without a step.  Got:\n%s"
                     % out0)
        if not re.search(r"walk: x=19 y=18 facing=1", out0):
            sys.exit("teleport_event_test: spawn position wrong: %r"
                     % out0.strip())

        # One forward step onto (20,18) fires SPEC7[13] -> cell 629 = (17,17).
        out1 = run(save, FORWARD)
        if PROMPT not in out1:
            sys.exit("teleport_event_test: the Air Elemental prompt is missing.  "
                     "Got:\n%s" % out1)
        if TELEPORT not in out1:
            sys.exit("teleport_event_test: the teleport did not fire.  Got:\n%s"
                     % out1)
        if FINAL not in out1:
            sys.exit("teleport_event_test: the party did not land on cell 629 "
                     "= (17,17).  Got:\n%s" % out1)
    finally:
        shutil.rmtree(workdir, ignore_errors=True)

    print("teleport_event_test: ok (SPEC7[13] teleports (19,18)->cell 629=(17,17))")


if __name__ == "__main__":
    main()
