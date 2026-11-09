#!/usr/bin/env python3
"""port_menu_test — regression test for the native LOF drop-in's menu screen.

Renders the menu with the port binary and compares it against a DOSBox capture
(ref/oracle/menu.png).  This pins the two things that were wrong until they were
recovered from the oracle:

  * the palette — the game programs the *standard* EGA attribute set, so index 6
    is brown and index 8 dark gray (a naive `i+8` remap renders green/yellow);
  * the menu text colours — the greeting and "(C)ontinue" are white on the gray
    panel, the option list is black.

Run via `make golden-port` (or directly).  Needs PIL + numpy and the oracle PNG.
"""
import os
import subprocess
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ORACLE = os.path.join(ROOT, "ref", "oracle", "menu.png")
SHOT = "/tmp/port_menu_test.png"

# Regions where the engine paints dynamic content the port cannot reproduce
# offline (the top-left corner is overwritten by the loaded town view).  The
# comparison tolerates a small number of stray pixels there.
THRESHOLD = 300


def main() -> int:
    if not os.path.exists(ORACLE):
        print("port_menu_test: SKIP (no oracle capture)")
        return 0
    subprocess.run(
        [os.path.join(ROOT, "lof"), "--dir", os.path.join(ROOT, "game"),
         "--headless", "--shot", SHOT],
        check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    port = np.asarray(Image.open(SHOT).convert("RGB")).astype(int)
    oracle = np.asarray(Image.open(ORACLE).convert("RGB")).astype(int)
    if port.shape != oracle.shape:
        print("port_menu_test: FAIL (size %s != %s)" % (port.shape, oracle.shape))
        return 1

    diff = (np.abs(port - oracle).sum(2) > 90)
    n = int(diff.sum())
    # Report the biggest sub-region so a regression points at its cause.
    regions = {"left box": (6, 135, 9, 132), "right menu": (135, 305, 90, 196),
               "top chrome": (0, 320, 0, 9), "bottom strip": (0, 320, 196, 200)}
    detail = ", ".join(
        "%s=%d" % (k, int(diff[y0:y1, x0:x1].sum()))
        for k, (x0, x1, y0, y1) in regions.items())
    if n <= THRESHOLD:
        print("port_menu_test: ok (diff=%d/%d; %s)" % (n, diff.size, detail))
        return 0
    print("port_menu_test: FAIL (diff=%d/%d; %s)" % (n, diff.size, detail))
    return 1


if __name__ == "__main__":
    sys.exit(main())
