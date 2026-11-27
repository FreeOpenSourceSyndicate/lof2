#!/usr/bin/env python3
"""port_dungeon_view_test — regression test for the depth-marched dungeon view.

The first-person viewport is a *pre-rendered sprite* selected by marching the
column of cells ahead of the party to the first view-opaque cell (see
`dungeon_view_for` in port/dungeon.{h,cpp} and docs/notes-a.md Round 47):

  * depth 1  -> EGA/D<d>/DP<index> at (10,11)  (the tileset view)
  * depth 2  -> EGA/CHAR2/CH40     at (10,11)
  * depth >=3-> EGA/CHAR2/CH25     at (10,11)

Every view cell is a full 120x119 image (the viewport size), so all depths blit
plainly at the viewport origin.

The D5/Temple save (slot 1) enters at DNG05 (17,3) facing S; the two DOSBox
oracle captures pin both regimes:

  * ref/oracle/dungeon_slot1.png       spawn (wall at depth 1) -> DP001
  * ref/oracle/dungeon_slot1_moved.png after a step (open corridor, wall at
                                       depth >=3) -> CH25 at (10,11)

`--walk` composes the HUD + party panel and blits that sprite.  This test renders
the spawn and one-step frames and checks the *viewport window* against each
oracle.  The window is checked (not the whole screen) because the oracle's HUD
panel is the engine's live state, which a save does not carry; the viewport is
the part this feature owns.  The moved case must match exactly, since the sprite
and its offset were recovered pixel-exactly.

Run via `make golden-port` (or directly).  Needs PIL + numpy and the oracle PNGs.
"""
import os
import subprocess
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, "game")
SAVE = os.path.join(GAME, "GAMES", "GAME1")
SPAWN_ORACLE = os.path.join(ROOT, "ref", "oracle", "dungeon_slot1.png")
MOVED_ORACLE = os.path.join(ROOT, "ref", "oracle", "dungeon_slot1_moved.png")

# Viewport window (matches kViewportX/Y/W/H in port/dungeon.h).
VX, VY, VW, VH = 10, 11, 120, 119

# The spawn window carries the engine's live overlay residue; the moved window
# is reproduced pixel-exactly as the `CHAR2/CH25` cell blitted at the viewport
# origin (see BLK-A005).  The only residual in the moved window is a 1px
# diagonal at the top-left corner (the engine's live element, absent from the
# emulator too), so the floor sits just below 1.0 to catch regressions.
SPAWN_MIN = 0.93
MOVED_MIN = 0.998


def walk(keys, shot):
    subprocess.run(
        [os.path.join(ROOT, "lof"), "--dir", GAME, "--headless", "--walk",
         "--save", SAVE, "--walk-keys", keys, "--shot", shot],
        check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def tiled(shot):
    subprocess.run(
        [os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
         "--play", "auto", "--tiled", "--save", SAVE, "--shot", shot],
        check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def load(path):
    return np.asarray(Image.open(path).convert("RGB")).astype(int)


def window(img):
    return img[VY:VY + VH, VX:VX + VW]


def main() -> int:
    if not (os.path.exists(SAVE) and os.path.exists(SPAWN_ORACLE)
            and os.path.exists(MOVED_ORACLE)):
        print("port_dungeon_view_test: SKIP (no game data / oracle)")
        return 0

    rc = 0

    walk("", "/tmp/port_dungeon_spawn.png")
    m = float((window(load("/tmp/port_dungeon_spawn.png"))
               == window(load(SPAWN_ORACLE))).all(2).mean())
    if m >= SPAWN_MIN:
        print("port_dungeon_view_test: spawn ok (window=%.1f%%, DP001)" % (100 * m))
    else:
        print("port_dungeon_view_test: FAIL spawn (window=%.1f%% < %.0f%%)"
              % (100 * m, 100 * SPAWN_MIN))
        rc = 1

    # Table-driven tiler (BLK-A005): --play --tiled composes the viewport from the
    # ray-cast + march + dispatch + slot geometry, matching the DOSBox spawn
    # oracle more closely than the single-sprite DP001 blit.
    tiled("/tmp/port_dungeon_tiled.png")
    m = float((window(load("/tmp/port_dungeon_tiled.png"))
               == window(load(SPAWN_ORACLE))).all(2).mean())
    if m >= 0.98:
        print("port_dungeon_view_test: tiled ok (window=%.1f%%, table-driven tiler)"
              % (100 * m))
    else:
        print("port_dungeon_view_test: FAIL tiled (window=%.1f%% < 98%%)"
              % (100 * m))
        rc = 1

    walk("0x48", "/tmp/port_dungeon_moved.png")   # step south -> open corridor
    m = float((window(load("/tmp/port_dungeon_moved.png"))
               == window(load(MOVED_ORACLE))).all(2).mean())
    if m >= MOVED_MIN:
        print("port_dungeon_view_test: moved ok (window=%.1f%%, CH25 @ (10,11))"
              % (100 * m))
    else:
        print("port_dungeon_view_test: FAIL moved (window=%.1f%% < %.0f%%)"
              % (100 * m, 100 * MOVED_MIN))
        rc = 1

    return rc


if __name__ == "__main__":
    sys.exit(main())
