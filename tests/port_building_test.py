#!/usr/bin/env python3
"""port_building_test — regression test for the native drop-in's building screens.

The town buildings all share one screen shape (see `compose_building_screen` in
port/lof.cpp): the static HUD background `EGA/UP/MSCR`, a town-scene tile in the
120x119 viewport at (10,11), then `CHR1` text at x=139.  Only the tile and the
text rows differ.  This test renders each building with the port and compares it
against the emulator's frame for that building:

  --emporium  EGA/TWN/TWN01  (the shop)          ref/oracle/emporium.png
  --bank      EGA/TWN/TWN02  (the bank)          ref/oracle/bank.png
  --house     EGA/TWN/TWN04  (the House of trades) ref/oracle/house.png

The references are generated from the emulator's `shop`/`bank`/`house` golden
frames (tests/golden_test.sh scenarios).  The viewport and every text row are
pixel-exact; the only residue is the engine's 16x16 top-left patch and the
dynamic party panel, both excluded.

Run via `make golden-port` (or directly).  Needs PIL + numpy and the reference
PNGs.
"""
import os
import subprocess
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, "game")

# flag -> (reference PNG, viewport-tile label used in messages)
BUILDINGS = [
    ("--emporium", "emporium.png", "Emporium (TWN01)"),
    ("--bank", "bank.png", "Bank (TWN02)"),
    ("--house", "house.png", "House of trades (TWN04)"),
]

MIN_MATCH = 0.99


def check(flag, ref, label):
    oracle_path = os.path.join(ROOT, "ref", "oracle", ref)
    if not os.path.exists(oracle_path):
        print("port_building_test: SKIP %s (no reference %s)" % (label, ref))
        return 0
    shot = "/tmp/port_building_%s.png" % ref
    subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                    flag, "--shot", shot],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    port = np.asarray(Image.open(shot).convert("RGB")).astype(int)
    oracle = np.asarray(Image.open(oracle_path).convert("RGB")).astype(int)
    if port.shape != oracle.shape:
        print("port_building_test: FAIL %s (size %s != %s)"
              % (label, port.shape, oracle.shape))
        return 1

    # The viewport and every text row must be byte-identical (this pins the EGA
    # decoder, the font mapping and the 6-px advance).
    for name, (ys, xs) in {
            "viewport": (slice(17, 130), slice(10, 130)),
            "text": (slice(88, 180), slice(139, 300)),
    }.items():
        if not (port[ys, xs] == oracle[ys, xs]).all():
            print("port_building_test: FAIL %s (%s region not exact)" % (label, name))
            return 1

    # Whole screen, masking the 16x16 top-left patch and the dynamic party panel.
    mask = np.ones(port.shape[:2], dtype=bool)
    mask[0:16, 0:16] = False
    mask[0:52, 140:320] = False
    match = float((port == oracle).all(2)[mask].mean())
    if match < 0.999:
        print("port_building_test: FAIL %s (static match=%.2f%% < 99.9%%)"
              % (label, 100 * match))
        return 1
    print("port_building_test: ok %s (viewport + text exact)" % label)
    return 0


def main() -> int:
    rc = 0
    for flag, ref, label in BUILDINGS:
        rc |= check(flag, ref, label)
    return rc


if __name__ == "__main__":
    sys.exit(main())
