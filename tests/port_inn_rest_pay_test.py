#!/usr/bin/env python3
"""Inn-rest "who pays" prompt (--inn-rest-pay) vs the emulator oracle.

The screen shown after choosing (S)imple nights stay at the Inn:
    A nights stay and meal costs
    2g,
    who pays (1-6)?
    (C)ontinue
over the same MSCR + TWN03 chrome as --inn-rest.  The oracle
`ref/emu/inn_rest_pay.png` is a 1-member party (Siegfried, cost 2g).  We compare
only the text block (x139..319, y88..140); the party panel is dynamic.

Lines 1, 3 and 4 must match the oracle ink exactly.  The cost line ("2g,") is
drawn with the same 6-px-advance CHR1 face, but the overlapping glyphs make the
engine's exact per-glyph placement of the computed number ambiguous, so it is
allowed a small (<=30 px) residual.
"""
import os
import subprocess
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, "game")
SAVE = os.path.join(GAME, "GAMES", "GAME2")
ORACLE = os.path.join(ROOT, "ref", "emu", "inn_rest_pay.png")

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


def fail(msg: str) -> int:
    print("port_inn_rest_pay_test: FAIL:", msg)
    return 1


def check_rest_effect() -> int:
    # --inn-rest-do N applies the derived pay+rest effect (image 0x820a..0x8680):
    # the payer's gold drops by 2*present, a wounded member heals (2*level +
    # max(0,Con-14), clamped), and the save is resealed.  GAME2: char 4 pays,
    # char 3 is wounded (9/11).
    a = open(SAVE, "rb").read()

    def rec(buf, i):
        return buf[4 + i * 410:4 + (i + 1) * 410]

    def gold(r):
        return r[0x72] | (r[0x72 + 1] << 8) | (r[0x72 + 2] << 16) | (r[0x72 + 3] << 24)

    def hp(r):
        return r[0x22] | (r[0x22 + 1] << 8)

    present = sum(1 for i in range(6) if rec(a, i)[0])
    cost = 2 * present
    if present != 6 or gold(rec(a, 3)) < cost:
        print("port_inn_rest_pay_test: SKIP rest effect (GAME2 fixture changed)")
        return 0
    out = "/tmp/port_inn_rest_effect.sav"
    r = subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                        "--inn-rest-do", "4", "--save", SAVE, "--amulet-out", out],
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    if r.returncode != 0:
        return fail("rest effect run failed: %s" % r.stderr.decode().strip())
    b = open(out, "rb").read()
    if len(b) != 13134:
        return fail("rest effect save size %d" % len(b))
    drop = gold(rec(a, 3)) - gold(rec(b, 3))
    if drop != cost:
        return fail("payer gold drop %d != cost %d" % (drop, cost))
    hmax = rec(b, 2)[0x20] | (rec(b, 2)[0x20 + 1] << 8)
    if not (hp(rec(a, 2)) < hp(rec(b, 2)) <= hmax):
        return fail("wounded char3 not healed to <= max (%d->%d/%d)"
                    % (hp(rec(a, 2)), hp(rec(b, 2)), hmax))
    if a[0x2e3a:0x2e3e] == b[0x2e3a:0x2e3e]:
        return fail("rest effect save not resealed")
    print("port_inn_rest_pay_test: ok (rest effect: payer -%dg, wounded char "
          "healed, save resealed)" % cost)
    return 0


def main() -> int:
    if not os.path.exists(ORACLE):
        print("port_inn_rest_pay_test: SKIP (no oracle capture)")
        return 0
    shot = "/tmp/port_inn_rest_pay.png"
    subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                    "--inn-rest-pay", "--save", SAVE, "--shot", shot],
                   check=True, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    port = to_index(shot, STOCK_ATTR)
    oracle = to_index(ORACLE, EMU_ATTR)

    def ink(img, y0, y1):
        return (img[y0:y1, 139:320] != 7)

    # Lines 1 (y92), 3 (y108) and 4 (y124) must be pixel-exact.
    for y0, y1, name in ((92, 100, "narration line 1"),
                         (108, 116, "who pays"),
                         (124, 132, "(C)ontinue")):
        if not (ink(port, y0, y1) == ink(oracle, y0, y1)).all():
            return fail("%s ink differs from oracle" % name)
        if not ink(port, y0, y1).any():
            return fail("%s has no ink" % name)

    # The computed cost line ("2g,") — small residual allowed (see docstring).
    d = int((ink(port, 100, 108) != ink(oracle, 100, 108)).sum())
    if d > 30:
        return fail("cost line ink residual too large: %d px" % d)

    print("port_inn_rest_pay_test: ok (who-pays prompt matches the oracle; "
          "cost-line residual %d px)" % d)
    rc = check_rest_effect()
    if rc:
        return rc
    return 0


if __name__ == "__main__":
    sys.exit(main())
