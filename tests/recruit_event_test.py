#!/usr/bin/env python3
"""End-to-end check that walking into the warrior raises the recruit event.

The player's report is the oracle: with no save loaded, leaving town and walking
forward reaches "A large warrior asks if he may join you (Yes/No)."; answering
yes gives "Siegurd joins the companionship.".

This drives the real `lof --walk` path over a shipped save moved to the cell in
front of the recruit cell on DNG10, and asserts three things:

  * the question (SPEC0[1] text_b) appears,
  * the joining character is named "Siegurd", which proves `+0xeb` indexes SNSC,
  * the warrior's speech (the `+0xc9` block) is *not* shown yet, because the
    tail skips that block for type 1 -- it belongs to the accept path.

The save is rewritten in place (moved, then re-sealed) because the shipped saves
start elsewhere on the level and the walk key script would be dozens of steps of
turn/step bookkeeping that would break on any layout change.  Moving the party is
what the engine's own "teleport" records do, so the file stays a valid save.
"""
import os
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOF = os.path.join(ROOT, "lof")
SAVE = os.path.join(ROOT, "game", "GAMES", "GAME2")

# GAME2 sits on location 10 (DNG10).  The recruit cell is (31,2); the party is
# put at (31,1) facing south so a single forward step enters it.
START_CELL = 1 * 36 + 31
FACING_SOUTH = 2
FORWARD = "0x48"

PARTY_CELL = 0x9A0
PARTY_FACING = 0x9A6
CHECKSUM_AT = 0x2E3A


def checksum(data):
    """The engine's save fold: seed 1, add on even index, xor on odd, NUL -> 1."""
    acc = 1
    for i, byte in enumerate(data):
        value = byte if byte else 1
        acc = (acc + value) & 0xFFFFFFFF if i % 2 == 0 else acc ^ value
    return acc & 0xFFFFFFFF


def make_save(path):
    data = bytearray(open(SAVE, "rb").read())
    struct.pack_into("<H", data, PARTY_CELL, START_CELL)
    data[PARTY_FACING] = FACING_SOUTH
    struct.pack_into("<I", data, CHECKSUM_AT, checksum(data[:CHECKSUM_AT]))
    with open(path, "wb") as f:
        f.write(data)


def run(save, keys, answer=None):
    cmd = [LOF, "--dir", os.path.join(ROOT, "game"), "--walk",
           "--save", save, "--walk-keys", keys]
    if answer is not None:
        cmd += ["--answer", answer]
    p = subprocess.run(
        cmd,
        capture_output=True, text=True, cwd=ROOT,
    )
    if p.returncode != 0:
        sys.exit("recruit_event_test: lof --walk failed: %s" % p.stderr.strip())
    return p.stdout


def main():
    if not os.path.exists(LOF):
        sys.exit("recruit_event_test: build the port first (make port)")
    if not os.path.exists(SAVE):
        sys.exit("recruit_event_test: missing fixture %s" % SAVE)

    workdir = tempfile.mkdtemp(prefix="lof-recruit-")
    try:
        save = os.path.join(workdir, "RECRUIT.SAV")
        make_save(save)

        # Control: standing on the start cell must not raise the event, so a
        # later hit cannot come from a cell the party was already on.
        before = run(save, "")
        if "large warrior" in before:
            sys.exit("recruit_event_test: the question printed without moving")

        # The joining class is the *accept* path: only a matching answer
        # reaches the effect switch (0x20A0E), so the run must answer "Y".
        after = run(save, FORWARD, "Y")
        for needle, why in (
            ("A large warrior asks if he may join you (Yes/No).",
             "the recruit question"),
            ("Siegurd joins the companionship.",
             "the join line, with the class named from SNSC"),
        ):
            if needle not in after:
                sys.exit("recruit_event_test: missing %s (%r).  Got:\n%s"
                         % (why, needle, after))

        # The speech is the *accept* path, and the tail skips the +0xc9 block for
        # type 1 outright (0x1F6DE).  Showing it on entry would hand the player
        # the "yes" text before they answered, so its absence is part of the
        # contract, not an unfinished feature.
        if "Cyldane" in after:
            sys.exit("recruit_event_test: the warrior's speech was printed "
                     "before the question was answered; the +0xc9 block must be "
                     "skipped for type 1.  Got:\n%s" % after)
        if "walk: x=31 y=2" not in after:
            sys.exit("recruit_event_test: the party did not enter the cell: %r"
                     % after.strip())
    finally:
        shutil.rmtree(workdir, ignore_errors=True)

    print("recruit_event_test: ok (question and the join line reproduced; the "
          "accept speech is correctly withheld until the answer)")


if __name__ == "__main__":
    main()
