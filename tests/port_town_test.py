#!/usr/bin/env python3
"""port_town_test — regression test for the native LOF drop-in's town screen.

Composes the in-game town screen with the port (`--town`) and compares it
against the emulator's town frame (ref/oracle/town.png, the `town` golden
scenario: `A`,`2`,`C`,`C` walks the party out of the Inn into the town).  This
pins the layout recovered from that frame:

  * the town screen is the static HUD background (EGA/UP/MSCR) with the town
    scene EGA/TWN/TWN00 blitted into the 120x119 viewport at (10,11);
  * the message ("You enter the proud town of / Thyn. ...") and the building
    menu ((E)mporium, (B)ank, ... (C)ontinue) are drawn in CHR1's full face at
    x=139, rows 92..188.

The town *name* is not hardcoded: the engine (image 0x53E0) selects "Thyn." or
"Cyldane." from the 16-bit word at save offset 0x9a4 (nonzero -> Thyn), so the
test also pins that selector using the owner-supplied Cyldane save and its
oracle frame (ref/oracle/town_cyldane.png).

The Cyldane fixture is read from the *committed* root save `GAME4`, not from
`game/GAMES/GAME4`: the owner also drops experimental saves into slot 4, and a
test fixture must not depend on whatever a slot happens to hold.

The static screen is pixel-exact except for the dynamic party panel the engine
draws from the loaded save.  The 16x16 top-left patch is the engine's software
mouse cursor (the capture parked the mouse at (0,0)); the test reproduces it
with `--cursor 0,0` and pins the corner too.

Run via `make golden-port` (or directly).  Needs PIL + numpy and the oracle PNG.
"""
import os
import subprocess
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, "game")
ORACLE = os.path.join(ROOT, "ref", "oracle", "town.png")
CYLDANE_SAVE = os.path.join(ROOT, "GAME4")
CYLDANE_ORACLE = os.path.join(ROOT, "ref", "oracle", "town_cyldane.png")
SHOT = "/tmp/port_town_test.png"
SHOT_CYLDANE = "/tmp/port_town_test_cyldane.png"

# The message block (rows 92..115) holds the "You enter the proud town of /
# <name>. Which building ..." text.  The Cyldane oracle uses a faded palette
# (it was captured on a different entry route), so that check compares ink
# masks, which are palette-independent.
MSG = (slice(92, 116), slice(139, 300))


def ink_mask(a: np.ndarray) -> np.ndarray:
    """Palette-independent "is this pixel ink" mask (vs the modal background)."""
    flat = a.reshape(-1, 3)
    vals, counts = np.unique(flat, axis=0, return_counts=True)
    bg = vals[counts.argmax()]
    return (np.abs(a - bg).sum(2) > 30)

# The viewport (MSCR + TWN00 at (10,11)) and every text row are pixel-exact; the
# residue is a 16x16 top-left patch (88 px) and the dynamic party panel.  Fail
# below this so a real regression in the composition or font mapping is caught.
MIN_MATCH = 0.99


def main() -> int:
    if not os.path.exists(ORACLE):
        print("port_town_test: SKIP (no oracle capture)")
        return 0
    subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME,
                    "--headless", "--town", "--cursor", "0,0", "--shot", SHOT],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    port = np.asarray(Image.open(SHOT).convert("RGB")).astype(int)
    oracle = np.asarray(Image.open(ORACLE).convert("RGB")).astype(int)
    if port.shape != oracle.shape:
        print("port_town_test: FAIL (size %s != %s)" % (port.shape, oracle.shape))
        return 1

    # The static composition: viewport + every text row must be pixel-exact.
    # The only excluded region is the party panel (top-right, drawn from live
    # save state).  The 16x16 top-left patch is the engine's software mouse
    # cursor (the DOSBox capture parked the mouse at (0,0)); `--cursor 0,0`
    # reproduces it, so the corner is now pinned too.
    mask = np.ones(port.shape[:2], dtype=bool)
    mask[0:52, 140:320] = False   # dynamic party panel
    static_match = float((port[mask] == oracle[mask]).all(1).mean())
    if static_match < 0.999:
        print("port_town_test: FAIL (static match=%.2f%% < 99.9%%)" % (100 * static_match))
        return 1
    if not (port[0:16, 0:16] == oracle[0:16, 0:16]).all():
        print("port_town_test: FAIL (mouse-cursor corner not exact)")
        return 1

    # The viewport must be byte-identical (the EGA decoder renders TWN00
    # exactly) and the message + menu text must be byte-identical (the font
    # mapping and 6-px advance are pinned by this).
    for name, (ys, xs) in {
            "viewport": (slice(17, 130), slice(10, 130)),
            "text": (slice(92, 196), slice(139, 300)),
    }.items():
        if not (port[ys, xs] == oracle[ys, xs]).all():
            print("port_town_test: FAIL (%s region not exact)" % name)
            return 1

    match = float((port == oracle).all(2).mean())
    if match < MIN_MATCH:
        print("port_town_test: FAIL (match=%.2f%% < %.0f%%)" % (100 * match, 100 * MIN_MATCH))
        return 1

    # The town-name selector: GAME4 sits in front of Cyldane, so the port must
    # print "Cyldane." there, and the message block must match the Cyldane oracle
    # (ink masks, since that oracle uses a faded palette).
    if os.path.exists(CYLDANE_SAVE) and os.path.exists(CYLDANE_ORACLE):
        subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                        "--town", "--save", CYLDANE_SAVE, "--shot", SHOT_CYLDANE],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        cyl = np.asarray(Image.open(SHOT_CYLDANE).convert("RGB")).astype(int)
        if (cyl[MSG] == port[MSG]).all():
            print("port_town_test: FAIL (Cyldane save renders the Thyn message)")
            return 1
        cyl_oracle = np.asarray(Image.open(CYLDANE_ORACLE).convert("RGB")).astype(int)
        if not (ink_mask(cyl)[MSG] == ink_mask(cyl_oracle)[MSG]).all():
            print("port_town_test: FAIL (Cyldane message block != oracle)")
            return 1
        print("port_town_test: ok (match=%.1f%%, viewport + text exact; Cyldane "
              "selector exact)" % (100 * match))
        return 0

    print("port_town_test: ok (match=%.1f%%, viewport + text exact)" % (100 * match))
    return 0


if __name__ == "__main__":
    sys.exit(main())
