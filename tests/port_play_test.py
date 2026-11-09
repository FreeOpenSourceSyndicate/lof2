#!/usr/bin/env python3
"""port_play_test — regression test for the native LOF drop-in's play screen.

Composes the in-game play screen with the port (`--play D0/DP001`) and compares
it against a DOSBox capture (ref/oracle/play.png, save slot 2, the wilderness).
This pins the two layout facts recovered from the oracle:

  * the play screen is the static HUD background (EGA/UP/MSCR) with the
    first-person viewport tile (EGA/D0/DP001) blitted into the 120x119 window at
    (10,11);
  * the port's EGA decoder renders DP001 pixel-exactly (verified separately), so
    the only tolerated residue is the engine's dynamic overlays inside the
    viewport (a band near the horizon) and the party/status text it draws with
    its own font.

Run via `make golden-port` (or directly).  Needs PIL + numpy and the oracle PNG.
"""
import os
import subprocess
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ORACLE = os.path.join(ROOT, "ref", "oracle", "play.png")
SHOT = "/tmp/port_play_test.png"
VIEWPORT = "D0/DP001"

# The static composition (MSCR + DP001 at (10,11)) matches the DOSBox capture at
# ~92%; the residue is the engine's dynamic overlays, which the port cannot
# reproduce offline.  Fail if the match drops below this fraction.
MIN_MATCH = 0.90


def main() -> int:
    if not os.path.exists(ORACLE):
        print("port_play_test: SKIP (no oracle capture)")
        return 0
    subprocess.run(
        [os.path.join(ROOT, "lof"), "--dir", os.path.join(ROOT, "game"),
         "--headless", "--play", VIEWPORT, "--shot", SHOT],
        check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    port = np.asarray(Image.open(SHOT).convert("RGB")).astype(int)
    oracle = np.asarray(Image.open(ORACLE).convert("RGB")).astype(int)
    if port.shape != oracle.shape:
        print("port_play_test: FAIL (size %s != %s)" % (port.shape, oracle.shape))
        return 1

    match = float((port == oracle).all(2).mean())
    if match >= MIN_MATCH:
        print("port_play_test: ok (match=%.1f%%, min=%.0f%%)" % (100 * match, 100 * MIN_MATCH))
        return 0
    print("port_play_test: FAIL (match=%.1f%% < %.0f%%)" % (100 * match, 100 * MIN_MATCH))
    return 1


if __name__ == "__main__":
    sys.exit(main())
