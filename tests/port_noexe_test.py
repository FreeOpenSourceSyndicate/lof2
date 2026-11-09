#!/usr/bin/env python3
"""port_noexe_test — the drop-in must run with the original EXEs absent.

The project's hardest constraint is that `lof`/`vor` never execute (or need)
the original `LOF.EXE` / `VOR.EXE`.  This test enforces it structurally: it
builds a game directory that contains every data file but *no* `.EXE`, points
the drop-in at it, and checks the menu still renders correctly against the
DOSBox oracle.

A drop-in that secretly reads or runs the originals would either crash here or
fail the oracle comparison.

Run via `make golden-noexe` (or directly).  Needs PIL + numpy and the oracle.
"""
import os
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, "game")
ORACLE = os.path.join(ROOT, "ref", "oracle", "menu.png")
BIN = os.path.join(ROOT, "lof")


def build_exeless_game(tmp):
    """Symlink every game entry except the original executables."""
    made = 0
    for name in os.listdir(GAME):
        if name.upper().endswith((".EXE", ".COM")):
            continue
        os.symlink(os.path.join(GAME, name), os.path.join(tmp, name))
        made += 1
    return made


def main():
    if not os.path.exists(BIN):
        print("port_noexe_test: SKIP (no ./lof — run `make port`)")
        return 0
    if not os.path.exists(ORACLE):
        print("port_noexe_test: SKIP (no oracle capture)")
        return 0
    if not os.path.isdir(GAME):
        print("port_noexe_test: SKIP (no game data — run ./bootstrap.sh)")
        return 0

    shot = os.path.join(tempfile.gettempdir(), "port_noexe_test.png")
    with tempfile.TemporaryDirectory() as tmp:
        n = build_exeless_game(tmp)
        # Sanity: the tree really has no executable in it.
        if any(f.upper().endswith((".EXE", ".COM")) for f in os.listdir(tmp)):
            print("port_noexe_test: FAIL (temp game dir still has an EXE)")
            return 1
        r = subprocess.run(
            [BIN, "--dir", tmp, "--headless", "--shot", shot],
            capture_output=True, text=True, timeout=120,
        )
        if r.returncode != 0:
            print("port_noexe_test: FAIL (exit %d): %s" % (r.returncode, r.stderr.strip()))
            return 1

    port = np.asarray(Image.open(shot).convert("RGB")).astype(int)
    oracle = np.asarray(Image.open(ORACLE).convert("RGB")).astype(int)
    if port.shape != oracle.shape:
        print("port_noexe_test: FAIL (size %s != %s)" % (port.shape, oracle.shape))
        return 1
    d = (np.abs(port - oracle).sum(2) > 90)
    n = int(d.sum())
    if n > 200:  # same tolerance as port_menu_test
        print("port_noexe_test: FAIL (diff=%d/%d with no EXEs present)" % (n, d.size))
        return 1
    print("port_noexe_test: ok (rendered with no EXE present; diff=%d/%d)" % (n, d.size))
    return 0


if __name__ == "__main__":
    sys.exit(main())
