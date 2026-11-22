#!/usr/bin/env python3
"""End-to-end check that the walk loop raises dungeon events.

Drives the real `lof --walk` path over a shipped save and asserts that stepping
onto an event cell prints the SPEC record the dispatcher selects, and that the
same run without the step does not.  The record text is the expected output, not
a stub: SYST/SPEC5 record 3, type 0 (plain text), text_b.

The control matters.  A test that only checks "the text appears somewhere" would
pass if the text were printed unconditionally; the no-step run proves the text
is tied to entering the cell.

Both readings of the SPEC index were checked against this level: SPEC5 is
selected by the ones digit of the location (DNG53 -> 5).  See the comment on
`spec_scenario_for_location` in port/char.h for the emulator trace that pins it.
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOF = os.path.join(ROOT, "lof")
SAVE = os.path.join(ROOT, "game", "GAMES", "GAME1")
EXPECTED = "You enter the acolyte lodgings."
DOOR_TEXT = "the wilderness begins, do you want to turn back (Yes/No)?"


def run(keys):
    p = subprocess.run(
        [LOF, "--dir", os.path.join(ROOT, "game"), "--walk",
         "--save", SAVE, "--walk-keys", keys],
        capture_output=True, text=True, cwd=ROOT,
    )
    if p.returncode != 0:
        sys.exit("event_fire_test: lof --walk failed: %s" % p.stderr.strip())
    return p.stdout


def main():
    if not os.path.exists(LOF):
        sys.exit("event_fire_test: build the port first (make port)")
    if not os.path.exists(SAVE):
        sys.exit("event_fire_test: missing fixture %s" % SAVE)

    before = run("")
    if EXPECTED in before:
        sys.exit("event_fire_test: event text printed without moving -- the "
                 "effect is not tied to entering the cell")

    after = run("0x48")
    if EXPECTED not in after:
        sys.exit("event_fire_test: stepping onto the event cell did not print "
                 "%r; got:\n%s" % (EXPECTED, after))

    # The step must also have moved the party, otherwise "it printed" could come
    # from a cell the party was already standing on.
    if "walk: x=17 y=3" not in before:
        sys.exit("event_fire_test: unexpected start position: %r" % before.strip())
    if "walk: x=17 y=4" not in after:
        sys.exit("event_fire_test: party did not move: %r" % after.strip())

    # Every record type reaches the text_b print at 0x1F713, not just the
    # plain-text group: the print sits after the 17-entry type dispatch, so an
    # interactive event still shows its prompt.  DNG05 cell (17,1) is a type-7
    # door, and its question must appear even though the effect itself is not
    # implemented yet.  Regression guard: printing text_b only for "text" types
    # made this door silent.
    door = run("0x50,0x48,0x48")
    if DOOR_TEXT not in door:
        sys.exit("event_fire_test: the type-7 door prompt was not printed; "
                 "text_b must be shown for every type.  Got:\n%s" % door)
    print("event_fire_test: ok (event fires on entering the cell, not before; "
          "non-text types still print their prompt)")


if __name__ == "__main__":
    main()
