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
  --temple    EGA/TWN/TWN05  (the Temple)        ref/oracle/temple_bldg.png

The references are generated from the emulator's `shop`/`bank`/`house`/
`temple_bldg` golden frames (tests/golden_test.sh scenarios).  The viewport and
every text row are pixel-exact; the only residue is the engine's 16x16 top-left
patch and the dynamic party panel, both excluded.  The Temple viewport is the
same two-tile scrolled blit the menu uses (see `blit_wrapped_viewport`), with the
tile's column 0 landing at viewport x=40.

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
GAME4 = os.path.join(GAME, "GAMES", "GAME4")

# flag -> (reference PNG, viewport-tile label used in messages)
BUILDINGS = [
    ("--emporium", "emporium.png", "Emporium (TWN01)"),
    ("--bank", "bank.png", "Bank (TWN02)"),
    ("--house", "house.png", "House of trades (TWN04)"),
    ("--temple", "temple_bldg.png", "Temple (TWN05)"),
    # The Temple's (H)eal / (R)evive options open a character-selection
    # sub-prompt over the *same* TWN05 viewport; only the greeting/option text
    # changes.  The question strings are the engine's own.
    ("--temple-heal", "temple_heal.png", "Temple heal prompt (TWN05)"),
    ("--temple-revive", "temple_revive.png", "Temple revive prompt (TWN05)"),
]

MIN_MATCH = 0.99


def check(flag, ref, label):
    oracle_path = os.path.join(ROOT, "ref", "oracle", ref)
    if not os.path.exists(oracle_path):
        print("port_building_test: SKIP %s (no reference %s)" % (label, ref))
        return 0
    shot = "/tmp/port_building_%s.png" % ref
    subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                    flag, "--cursor", "0,0", "--shot", shot],
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

    # Whole screen, masking only the dynamic party panel.  The 16x16 top-left
    # patch is the engine's software mouse cursor (capture parked at (0,0)),
    # reproduced by `--cursor 0,0`, so the corner is pinned too.
    mask = np.ones(port.shape[:2], dtype=bool)
    mask[0:52, 140:320] = False
    match = float((port == oracle).all(2)[mask].mean())
    if match < 0.999:
        print("port_building_test: FAIL %s (static match=%.2f%% < 99.9%%)"
              % (label, 100 * match))
        return 1
    if not (port[0:16, 0:16] == oracle[0:16, 0:16]).all():
        print("port_building_test: FAIL %s (mouse-cursor corner not exact)" % label)
        return 1
    print("port_building_test: ok %s (viewport + text + cursor exact)" % label)
    return 0


def check_temple_heal_effect() -> int:
    # --temple-heal-do N applies the derived heal (image 0x9f88..0xab6a): cost =
    # max(1, (hp_max-hp_cur)*level/2), hp_cur set to hp_max, paid from the payer.
    if not os.path.exists(GAME4):
        print("port_building_test: SKIP temple-heal effect (no GAME4)")
        return 0

    def rec(buf, i):
        return buf[4 + i * 410:4 + (i + 1) * 410]

    def hp(r):
        return r[0x22] | (r[0x22 + 1] << 8)

    def hpmax(r):
        return r[0x20] | (r[0x20 + 1] << 8)

    def gold(r):
        return r[0x72] | (r[0x72 + 1] << 8) | (r[0x72 + 2] << 16) | (r[0x72 + 3] << 24)

    a = open(GAME4, "rb").read()
    r0 = rec(a, 0)
    cost = max(1, (hpmax(r0) - hp(r0)) * r0[0x17] // 2) if hp(r0) < hpmax(r0) else 0
    if cost == 0 or gold(r0) < cost:
        print("port_building_test: SKIP temple-heal effect (GAME4 c1 not healable)")
        return 0
    out = "/tmp/port_temple_heal.sav"
    r = subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                        "--temple-heal-do", "1", "--save", GAME4,
                        "--amulet-out", out],
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    if r.returncode != 0:
        print("port_building_test: FAIL temple-heal effect (%s)"
              % r.stderr.decode().strip())
        return 1
    b = open(out, "rb").read()
    r0b = rec(b, 0)
    if hp(r0b) != hpmax(r0b):
        print("port_building_test: FAIL temple-heal effect (hp %d/%d not full)"
              % (hp(r0b), hpmax(r0b)))
        return 1
    if gold(r0) - gold(r0b) != cost:
        print("port_building_test: FAIL temple-heal effect (paid %d != %d)"
              % (gold(r0) - gold(r0b), cost))
        return 1
    if len(b) != 13134 or a[0x2e3a:0x2e3e] == b[0x2e3a:0x2e3e]:
        print("port_building_test: FAIL temple-heal effect (save not resealed)")
        return 1
    print("port_building_test: ok temple-heal effect (hp->full, paid %dg, "
          "resealed)" % cost)
    return 0


def main() -> int:
    rc = 0
    for flag, ref, label in BUILDINGS:
        rc |= check(flag, ref, label)
    rc |= check_temple_heal_effect()
    return rc


if __name__ == "__main__":
    sys.exit(main())
