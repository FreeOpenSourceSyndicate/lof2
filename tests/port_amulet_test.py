#!/usr/bin/env python3
"""Verify `lof --amulet` -- the give-artefact page shown when leaving town.

The page reuses the play-screen chrome (MSCR + the town viewport + the party
panel) and prints the engine's narration in the right-hand text block (CHR1,
x=139): "On starting your journey, you receive an amulet from a town guard. A
gift from the Count." (wrapped to three 29-cell lines), "You find an artefact:",
"The Amulet", "Who should receive that (1-6)?", "(L)eave it".

The archived oracle ref/emu/amulet_newgame.png is an *emulator* capture (its
palette is the emulator's, not the port's stock DOSBox set), so the comparison
is by **ink mask** (non-background pixels), which is palette-independent -- the
same method tests/port_round_result_test.py uses.  The text region (x139..319,
y92..171) is uniform MSCR index 7 except the glyphs, so the ink mask is exact.

The viewport (TWN00 + a town-guard sprite overlay) and the party panel are not
asserted here; only the derived text block is.
"""
import os
import subprocess
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, "game")
ORACLE = os.path.join(ROOT, "ref", "emu", "amulet_newgame.png")
SAVE = os.path.join(GAME, "GAMES", "GAME2")
SHOT = "/tmp/port_amulet_test.png"

# Text region: below the party panel (y < 92) and right of the viewport.
TEXT = (slice(92, 172), slice(139, 320))

EMU_ATTR = [0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17]
STOCK_ATTR = [0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07,
              0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F]


def rgb_from_attr(attr: int):
    attr &= 0x3F
    return (((attr >> 2 & 1) * 170 + (attr >> 5 & 1) * 85),
            ((attr >> 1 & 1) * 170 + (attr >> 4 & 1) * 85),
            ((attr & 1) * 170 + (attr >> 3 & 1) * 85))


def to_index(path: str, attrs) -> np.ndarray:
    a = np.asarray(Image.open(path).convert("RGB")).astype(int)
    flat = a.reshape(-1, 3)
    pal = np.array([rgb_from_attr(v) for v in attrs])
    out = np.full(len(flat), -1)
    for i, c in enumerate(pal):
        out[(flat == c).all(1)] = i
    return out.reshape(a.shape[0], a.shape[1])


def render(shot: str) -> np.ndarray:
    cmd = [os.path.join(ROOT, "lof"), "--dir", GAME, "--headless", "--amulet",
           "--save", SAVE, "--shot", shot]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    return to_index(shot, STOCK_ATTR)


def ink(img, region):
    return img[region] != 7


def fail(msg: str) -> int:
    print("port_amulet_test: FAIL (%s)" % msg)
    return 1


def check_give() -> int:
    # `--amulet-give N` reproduces the engine's item-add (0x69e2..0x6c5f): the
    # amulet (item 120) lands in the first empty inventory slot of character N,
    # condition 100, and the used-slot count (+0x6b) is bumped by one.
    out = "/tmp/port_amulet_give.sav"
    cmd = [os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
           "--amulet-give", "1", "--save", SAVE, "--amulet-out", out]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    a = open(SAVE, "rb").read()
    b = open(out, "rb").read()

    def raw(buf, i):
        return buf[4 + i * 410:4 + (i + 1) * 410]

    r0a, r0b = raw(a, 0), raw(b, 0)
    if r0a[0] != 1:
        return fail("save fixture char 0 not present")
    added = False
    for k in range(48):
        o = 0x82 + k * 4
        if r0a[o] == 0 and r0b[o] == 120 and r0b[o + 3] == 100:
            added = True
    if not added:
        return fail("give: amulet (item 120) not added to an empty slot")
    if r0b[0x6b] != r0a[0x6b] + 1:
        return fail("give: inv_used not incremented (%d -> %d)"
                    % (r0a[0x6b], r0b[0x6b]))
    return 0


def main() -> int:
    if not os.path.exists(ORACLE):
        print("port_amulet_test: SKIP (no oracle capture)")
        return 0
    if not os.path.exists(SAVE):
        print("port_amulet_test: SKIP (no save fixture)")
        return 0

    port = render(SHOT)
    oracle = to_index(ORACLE, EMU_ATTR)
    if port.shape != oracle.shape:
        return fail("size %s != %s" % (port.shape, oracle.shape))

    # The page is the town page with only its right-hand text changed
    # (owner-confirmed in DOSBox): its upper-left viewport is the plain street
    # tile TWN00, with no separate guard sprite.  Compare it against the oracle
    # through the index maps (palette-independent); it should be ~1.0.
    vp = (slice(11, 130), slice(10, 130))
    vm = float((port[vp] == oracle[vp]).mean())
    if vm < 0.99:
        return fail("viewport match %.4f < 0.99 (expected plain TWN00 ~1.0)" % vm)

    pi, oi = ink(port, TEXT), ink(oracle, TEXT)
    if not (pi == oi).all():
        return fail("text ink differs (%d px)" % int((pi != oi).sum()))

    # Each of the seven lines must be individually inked (so a dropped or
    # misplaced line is caught, not just a coincidental match).
    for y0 in (92, 100, 108, 116, 132, 148, 164):
        if not (port[y0:y0 + 8, 139:320] != 7).any():
            return fail("line at y=%d has no ink" % y0)

    print("port_amulet_test: ok (text block ink matches the oracle exactly; "
          "viewport %.4f vs TWN00)" % vm)
    rc = check_give()
    if rc:
        return rc
    print("port_amulet_test: ok (--amulet-give adds The Amulet to the slot)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
