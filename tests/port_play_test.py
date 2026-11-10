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
GAME = os.path.join(ROOT, "game")
ORACLE = os.path.join(ROOT, "ref", "oracle", "play.png")
SHOT = "/tmp/port_play_test.png"
VIEWPORT = "D0/DP001"
SAVE = os.path.join(GAME, "GAMES", "GAME2")

# The composition (MSCR + DP001 at (10,11) + the full party panel drawn from the
# save: name, profession, the +0x1e stat number and the "GOOD" status word)
# matches the DOSBox capture at ~94.2%; the panel is now pixel-exact and the
# only residue is the engine's dynamic viewport overlays.  Fail below this.
MIN_MATCH = 0.94


def main() -> int:
    if not os.path.exists(ORACLE):
        print("port_play_test: SKIP (no oracle capture)")
        return 0
    cmd = [os.path.join(ROOT, "lof"), "--dir", GAME,
           "--headless", "--play", VIEWPORT, "--shot", SHOT]
    if os.path.exists(SAVE):
        cmd += ["--save", SAVE]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    port = np.asarray(Image.open(SHOT).convert("RGB")).astype(int)
    oracle = np.asarray(Image.open(ORACLE).convert("RGB")).astype(int)
    if port.shape != oracle.shape:
        print("port_play_test: FAIL (size %s != %s)" % (port.shape, oracle.shape))
        return 1

    match = float((port == oracle).all(2).mean())
    if match < MIN_MATCH:
        print("port_play_test: FAIL (match=%.1f%% < %.0f%%)" % (100 * match, 100 * MIN_MATCH))
        return 1

    # The lower-left status box must be pixel-exact where the save determines
    # its text: the scenario title (y=136) and the facing line (y=144) come from
    # the save's location/facing bytes, and the "Spirits"/"Rations" labels
    # (x=6..53) are constant.  The time-of-day, morale and rations *values* are
    # live engine state a save does not carry, so those columns are excluded.
    for y in (136, 144):
        if not (port[y:y + 8, 0:140] == oracle[y:y + 8, 0:140]).all():
            print("port_play_test: FAIL (status box line y=%d not exact)" % y)
            return 1
    for y in (176, 184):
        if not (port[y:y + 8, 6:54] == oracle[y:y + 8, 6:54]).all():
            print("port_play_test: FAIL (status box label y=%d not exact)" % y)
            return 1
    print("port_play_test: ok (match=%.1f%%, min=%.0f%%, status box exact)" %
          (100 * match, 100 * MIN_MATCH))
    return 0


if __name__ == "__main__":
    sys.exit(main())
