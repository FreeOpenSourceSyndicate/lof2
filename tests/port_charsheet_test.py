#!/usr/bin/env python3
"""port_charsheet_test — regression test for the native LOF drop-in's character
sheet (`--charsheet`).

The sheet is the play-screen chrome (EGA/UP/MSCR + the shared 3x2 party grid)
with the selected member's class portrait (EGA/CHAR3/CH<profession>) blitted at
the viewport origin, the location/status box in the sheet's green, and the stat
block on the right panel.  It is drawn under the char-sheet attribute palette
(the game reprograms the attribute controller: index 8 = dark green, 15 = pale
green), recovered from the emulator's register dump and verified against the
DOSBox capture ref/oracle/charsheet.png.

Assertions:
  * the 120x119 portrait region is pixel-exact (it is a single sprite blit);
  * the 3x2 party grid (names/professions/stat%/HT+HP gauges) is pixel-exact;
  * the stat-panel text region matches >= 99% (the tiny residue is the CHR1
    six-px-overlap artefact seen on every text screen, plus a handful of
    palette-index specks in the DOSBox capture);
  * the whole screen matches >= 99.5%.  The fixed residue is (a) the dynamic
    time-of-day line (the save says "Evening" while the oracle was captured at
    "Midnight!") and (b) the Spirits/Rations value dashes (BLK-M002,
    undecoded).

Run via `make golden-port` (or directly).  Needs PIL + numpy and the oracle PNG.
"""
import os
import subprocess
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, "game")
ORACLE = os.path.join(ROOT, "ref", "oracle", "charsheet.png")
SHOT = "/tmp/port_charsheet_test.png"
SAVE = os.path.join(GAME, "GAMES", "GAME2")

MIN_WHOLE = 0.995
MIN_STAT = 0.99


def main() -> int:
    if not os.path.exists(ORACLE):
        print("port_charsheet_test: SKIP (no oracle capture)")
        return 0
    cmd = [os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
           "--charsheet", "1", "--shot", SHOT]
    if os.path.exists(SAVE):
        cmd += ["--save", SAVE]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)

    port = np.asarray(Image.open(SHOT).convert("RGB")).astype(int)
    oracle = np.asarray(Image.open(ORACLE).convert("RGB")).astype(int)
    if port.shape != oracle.shape:
        print("port_charsheet_test: FAIL (size %s != %s)" % (port.shape, oracle.shape))
        return 1

    diff = (port != oracle).any(2)

    # Portrait: the class sprite blitted plainly at (10,11) must be exact.
    portrait = diff[11:130, 10:130]
    if portrait.any():
        print("port_charsheet_test: FAIL (portrait differs in %d px)" % int(portrait.sum()))
        return 1

    # The 3x2 party grid: every occupied slot cell must be pixel-exact (names,
    # professions, stat%, stats and the proportional HT/HP gauges).
    grid = diff[6:72, 140:318]
    if grid.any():
        print("port_charsheet_test: FAIL (party grid differs in %d px)" % int(grid.sum()))
        return 1

    # Stat panel text (below the divider, right of the viewport column).
    stat = diff[88:172, 140:320]
    stat_match = 1.0 - stat.mean()
    if stat_match < MIN_STAT:
        print("port_charsheet_test: FAIL (stat panel match=%.4f < %.2f)"
              % (stat_match, MIN_STAT))
        return 1

    whole = 1.0 - diff.mean()
    if whole < MIN_WHOLE:
        print("port_charsheet_test: FAIL (whole match=%.4f < %.3f)"
              % (whole, MIN_WHOLE))
        return 1

    print("port_charsheet_test: ok (whole=%.4f stat=%.4f portrait=exact)"
          % (whole, stat_match))
    return 0


if __name__ == "__main__":
    sys.exit(main())
