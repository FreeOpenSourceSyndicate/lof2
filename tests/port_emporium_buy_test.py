#!/usr/bin/env python3
"""port_emporium_buy_test — the Emporium 8-item buy list.

After the Emporium (B)uy option the engine asks "Who is interested in my wares
(1-6)?" and then draws the 8-slot stock list (image 0x6c60): a header
"<name>: Gold:<n>" + "Weight...: <cur>/<max>", eight rows "n <name> <price>$
<weight>lb" for the shop's carried items, then "(C)ontinue".

Compared to the DOSBox capture `ref/emu/emporium_buylist.png` (slot 4 -> town ->
Emporium -> B -> 1, character Conan).  The list text (x>=139) is pixel-exact;
the whole-screen residue is the dynamic party panel (excluded from the check).
"""
import os
import subprocess
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, "game")
GAME4 = os.path.join(GAME, "GAMES", "GAME4")
REF = os.path.join(ROOT, "ref", "emu", "emporium_buylist.png")


def main():
    out = "/tmp/emporium_buy_test.png"
    r = subprocess.run(
        [os.path.join(ROOT, "lof"), "--dir", GAME, "--emporium-buy", "1",
         "--save", GAME4, "--headless", "--shot", out],
        capture_output=True, text=True)
    if r.returncode != 0:
        print("emporium_buy: FAIL run:", r.stderr.strip())
        return 1
    a = np.asarray(Image.open(out).convert("RGB")).astype(int)
    b = np.asarray(Image.open(REF).convert("RGB")).astype(int)
    if a.shape != b.shape:
        print("emporium_buy: FAIL shape", a.shape, b.shape)
        return 1

    def match(x0, y0, w, h):
        return (a[y0:y0 + h, x0:x0 + w] == b[y0:y0 + h, x0:x0 + w]).all(2).mean()

    text = match(139, 88, 170, 100)
    ok = text >= 0.99
    print(f"emporium buy list: text match={text:.4f} ({'OK' if ok else 'FAIL'})")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
