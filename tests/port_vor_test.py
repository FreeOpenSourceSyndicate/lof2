#!/usr/bin/env python3
"""port_vor_test — regression test for the native VOR drop-in.

`vor` reproduces what START.BAT runs before LOF: the dragon title
(EGA/IN/DRAG) and the credits still (EGA/IN/CRED).  Unlike the menu/play
screens, the VOR stills are pure image blits with no engine overlay, so the
port must reproduce each one *pixel-exactly* from the game data.  This test
pins that against the decoded source assets (no DOSBox capture needed).

Run via `make golden-port` (or directly).  Needs PIL + numpy and the game data.
"""
import os
import subprocess
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "tools"))
import ega  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, "game")
BIN = os.path.join(ROOT, "vor")

# EGA default 16-colour palette, as the game programs it (see port/gfx.cpp).
STOCK = np.array([
    (0, 0, 0), (0, 0, 170), (0, 170, 0), (0, 170, 170),
    (170, 0, 0), (170, 0, 170), (170, 85, 0), (170, 170, 170),
    (85, 85, 85), (85, 85, 255), (85, 255, 85), (85, 255, 255),
    (255, 85, 85), (255, 85, 255), (255, 255, 85), (255, 255, 255),
], dtype=int)

FRAMES = [("DRAG", "EGA/IN/DRAG"), ("CRED", "EGA/IN/CRED")]


def to_indices(path):
    im = np.asarray(Image.open(path).convert("RGB")).astype(int)
    return ((im[:, :, None, :] - STOCK[None, None, :, :]) ** 2).sum(3).argmin(2)


def main() -> int:
    if not os.path.exists(BIN):
        print("port_vor_test: SKIP (no ./vor — run `make port`)")
        return 0
    if not os.path.isdir(GAME):
        print("port_vor_test: SKIP (no game data — run ./bootstrap.sh)")
        return 0

    for i, (name, asset) in enumerate(FRAMES):
        src = os.path.join(GAME, asset)
        if not os.path.exists(src):
            print("port_vor_test: SKIP (missing %s)" % asset)
            return 0
        shot = "/tmp/port_vor_test_%d.png" % i
        subprocess.run(
            [BIN, "--dir", GAME, "--headless", "--shot", shot,
             "--shot-frame", str(i)],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        port = to_indices(shot)
        want = np.array(list(ega.indices(src)))
        if port.shape != want.shape:
            print("port_vor_test: FAIL (%s size %s != %s)" % (name, port.shape, want.shape))
            return 1
        match = float((port == want).mean())
        if match < 1.0:
            n = int((port != want).sum())
            print("port_vor_test: FAIL (%s match=%.2f%%, %d px differ)" % (name, 100 * match, n))
            return 1
        print("port_vor_test: ok (%s matches %s exactly)" % (name, asset))
    return 0


if __name__ == "__main__":
    sys.exit(main())
