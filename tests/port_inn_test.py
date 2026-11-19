#!/usr/bin/env python3
"""port_inn_test — regression test for the native drop-in's Inn screens.

The town's Inn is the one building with no dedicated artwork: the engine's
`(I)nn` handler simply redisplays the boot **main menu** (the town screen's
`I` hot-key leaves the same menu the intro ends on).  This was confirmed by
driving the emulator into the Inn and diffing against the boot menu: the two
frames are byte-identical everywhere except the dynamic party panel.

  * `lof` with no screen flag renders that menu; `--inn-rest` renders the
    innkeeper's `(R)est` sub-prompt (over the *same* scrolled TWN03 viewport the
    menu uses), whose question and `(S)`/`(W)`/`(J)` options are the engine's own
    strings.

References are the emulator's `town_inn` (the Inn == menu) and `inn_rest`
frames, rendered with the standard EGA palette.  The viewport and every text
row are pixel-exact; the residue is the dynamic party panel.

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

# (port args, reference PNG, label)
SCREENS = [
    ([], "inn.png", "Inn == main menu"),
    (["--inn-rest"], "inn_rest.png", "Inn rest prompt"),
]

MIN_MATCH = 0.999


def check(extra, ref, label):
    oracle_path = os.path.join(ROOT, "ref", "oracle", ref)
    if not os.path.exists(oracle_path):
        print("port_inn_test: SKIP %s (no reference %s)" % (label, ref))
        return 0
    shot = "/tmp/port_inn_%s.png" % ref
    subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                    "--cursor", "0,0", "--shot", shot] + extra,
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    port = np.asarray(Image.open(shot).convert("RGB")).astype(int)
    oracle = np.asarray(Image.open(oracle_path).convert("RGB")).astype(int)
    if port.shape != oracle.shape:
        print("port_inn_test: FAIL %s (size %s != %s)"
              % (label, port.shape, oracle.shape))
        return 1

    # The viewport and every text row must be byte-identical.
    for name, (ys, xs) in {
            "viewport": (slice(17, 130), slice(10, 130)),
            "text": (slice(88, 196), slice(139, 300)),
    }.items():
        if not (port[ys, xs] == oracle[ys, xs]).all():
            print("port_inn_test: FAIL %s (%s region not exact)" % (label, name))
            return 1

    # Whole screen, masking the dynamic party panel.  The only other residue is
    # the 14-px viewport-strip seam the menu test already tolerates (it crosses
    # the top-left cursor corner, so the corner is not asserted separately here).
    mask = np.ones(port.shape[:2], dtype=bool)
    mask[0:52, 140:320] = False
    diff = (port != oracle).any(2)
    n = int((diff & mask).sum())
    if n > 20:
        print("port_inn_test: FAIL %s (residue %d px > 20)" % (label, n))
        return 1
    print("port_inn_test: ok %s (viewport + text exact, residue=%d)"
          % (label, n))
    return 0


def main() -> int:
    rc = 0
    for extra, ref, label in SCREENS:
        rc |= check(extra, ref, label)
    return rc


if __name__ == "__main__":
    sys.exit(main())
