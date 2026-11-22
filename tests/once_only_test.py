#!/usr/bin/env python3
"""The once-only event gate (dispatcher 0x1F603..0x1F62A).

The type word does not select a handler; the 17-word table at image 0x1F5CA
holds two IPs and only sets the flag `[bp-8]`.  That flag gates one check: when
a record has `+0xe4 == 1` and a type that takes the 0 flag, the dispatcher reads
`[0x37ee + record+0xe6]` and, if set, jumps to the shared epilogue (0x212F6) --
skipping the record entirely, text and all.

`[0x37ee]` is a 12-byte party-wide union of character byte `+0x38 + i` over the
six save slots (loops at 0x1F38C/0x1F396, stride 0x19a), not a global latch.

Exactly one record in the shipped data is gated: SPEC2[0], a type-7 record whose
text is "Above you is the hole through which you arrived here." -- an arrival
hole, which is precisely what a once-only record should be.  This test walks into
its cell with the flags set and clear and asserts the text is suppressed only in
the first case.
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

# SPEC2[0] sits at (26,5) on DNG02; approach from (26,6) facing north.
START_CELL = 6 * 36 + 26
FACING_NORTH = 0
FORWARD = "0x48"

PARTY_CELL = 0x9A0
PARTY_FACING = 0x9A6
LOC_ONES = 0x9A2
LOC_TENS = 0x9A4
CHECKSUM_AT = 0x2E3A
HEADER = 4
SLOT = 410
DONE_FLAGS = 0x38      # per-character byte array the union is built from
DONE_COUNT = 12

TEXT = "Above you is the hole through which you arrived here."


def checksum(data):
    acc = 1
    for i, byte in enumerate(data):
        value = byte if byte else 1
        acc = (acc + value) & 0xFFFFFFFF if i % 2 == 0 else acc ^ value
    return acc & 0xFFFFFFFF


def make_save(path, clear_flags):
    data = bytearray(open(SAVE, "rb").read())
    struct.pack_into("<H", data, PARTY_CELL, START_CELL)
    data[PARTY_FACING] = FACING_NORTH
    data[LOC_ONES] = 2      # location 2 -> DNG02, SYST/SPEC2
    data[LOC_TENS] = 0
    if clear_flags:
        for slot in range(6):
            base = HEADER + slot * SLOT
            if data[base] == 0:
                continue
            for i in range(DONE_COUNT):
                data[base + DONE_FLAGS + i] = 0
    struct.pack_into("<I", data, CHECKSUM_AT, checksum(data[:CHECKSUM_AT]))
    with open(path, "wb") as f:
        f.write(data)


def run(save):
    p = subprocess.run(
        [LOF, "--dir", os.path.join(ROOT, "game"), "--walk",
         "--save", save, "--walk-keys", FORWARD],
        capture_output=True, text=True, cwd=ROOT,
    )
    if p.returncode != 0:
        sys.exit("once_only_test: lof --walk failed: %s" % p.stderr.strip())
    return p.stdout


def main():
    if not os.path.exists(LOF):
        sys.exit("once_only_test: build the port first (make port)")
    if not os.path.exists(SAVE):
        sys.exit("once_only_test: missing fixture %s" % SAVE)

    workdir = tempfile.mkdtemp(prefix="lof-once-")
    try:
        # The shipped saves already have entries 1 and 10 set, and the record
        # being tested uses index 1 -- so "flags present" is the shipped state.
        done = os.path.join(workdir, "DONE.SAV")
        make_save(done, clear_flags=False)
        out_done = run(done)
        if TEXT in out_done:
            sys.exit("once_only_test: the once-only record printed even though "
                     "the party's +0x38[1] is already set.  Got:\n%s" % out_done)
        if "walk: x=26 y=5" not in out_done:
            sys.exit("once_only_test: the party did not reach the cell: %r"
                     % out_done.strip())

        fresh = os.path.join(workdir, "FRESH.SAV")
        make_save(fresh, clear_flags=True)
        out_fresh = run(fresh)
        if TEXT not in out_fresh:
            sys.exit("once_only_test: a fresh party must see the once-only "
                     "record.  Got:\n%s" % out_fresh)
    finally:
        shutil.rmtree(workdir, ignore_errors=True)

    print("once_only_test: ok (SPEC2[0] suppressed when the party's +0x38[1] "
          "is set, shown when clear)")


if __name__ == "__main__":
    main()
