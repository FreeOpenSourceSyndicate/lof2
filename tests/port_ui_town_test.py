#!/usr/bin/env python3
"""port_ui_town_test — regression test for the --ui town hub (P-4).

When `--ui` opens a save whose location has no DNGS map, the engine "stays in
town": it shows the building menu instead of a first-person view.  The port
mirrors that — `--ui` enters a TOWN state (MSCR + TWN00 + "Which building do you
want to visit?" + (E)/(B)/(I)/(H)/(T)/(C)) and the keys switch to the Emporium /
Bank / House / Temple screens, (I) returns to the boot menu (the Inn), and (C)
opens the amulet (leave-town) page.  Every one of those frames is byte-identical
to the matching standalone renderer (`--town` / `--emporium` / ... / `--amulet`),
so this pins the state-machine wiring without needing an oracle capture.

The in-town fixture is made by patching an archived save's location bytes
(ones 0x9a2 = 8, tens 0x9a4 = 0 -> DNGS/DNG08, which does not ship).  The port
loads it (warns about the broken seal) and, finding no map, stays in town.

Run via `make golden-port` (or directly).  Needs PIL + numpy and the game data.
"""
import os
import subprocess
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, "game")
SAVE_SRC = os.path.join(GAME, "GAMES", "GAME2")
TOWN_SAVE = "/tmp/port_ui_town.sav"
TOWN_EXIT_SAVE = "/tmp/port_ui_town_exit.sav"
UI_SHOT = "/tmp/port_ui_town_shot.png"
REF_SHOT = "/tmp/port_ui_town_ref.png"


def make_town_save():
    d = bytearray(open(SAVE_SRC, "rb").read())
    d[0x9A2] = 8   # location ones  -> "08"
    d[0x9A4] = 0   # location tens
    open(TOWN_SAVE, "wb").write(d)


def ui(keys):
    subprocess.run(
        [os.path.join(ROOT, "lof"), "--dir", GAME, "--headless", "--ui",
         "--save", TOWN_SAVE, "--ui-keys", keys, "--shot", UI_SHOT],
        check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def ref(flag, with_save=False):
    cmd = [os.path.join(ROOT, "lof"), "--dir", GAME, "--headless", flag,
           "--shot", REF_SHOT]
    if with_save:
        cmd += ["--save", TOWN_SAVE]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)


def load(p):
    return np.asarray(Image.open(p).convert("RGB")).astype(int)


def check(name, keys, flag, with_save=False):
    ui(keys)
    a = load(UI_SHOT)
    ref(flag, with_save)
    b = load(REF_SHOT)
    ok = a.shape == b.shape and bool((a == b).all())
    print("port_ui_town_test: %s %s" % ("ok" if ok else "FAIL", name))
    return ok


def make_town_exit_save():
    """A save in town whose region is 1, so leaving town lands on DNG10 (the
    same outdoor map as the slot-2 save)."""
    d = bytearray(open(SAVE_SRC, "rb").read())
    d[0x9A2] = 8   # ones -> DNG18 (no map): the engine stays in town
    d[0x9A4] = 1   # tens -> outdoor map DNG10 (matches GAME2's own region)
    open(TOWN_EXIT_SAVE, "wb").write(d)


def check_town_exit():
    """(C)ontinue -> amulet page -> (L)eave it leaves town into the wilderness;
    the resulting play screen must equal a direct load of the wilderness save."""
    make_town_exit_save()
    subprocess.run(
        [os.path.join(ROOT, "lof"), "--dir", GAME, "--headless", "--ui",
         "--save", TOWN_EXIT_SAVE, "--ui-keys", "32,32,79,67,76", "--shot", UI_SHOT],
        check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    a = load(UI_SHOT)
    # Reference: load the wilderness save directly through the same --ui path
    # (its PLAY state) and shoot that play screen.
    subprocess.run(
        [os.path.join(ROOT, "lof"), "--dir", GAME, "--headless", "--ui",
         "--save", SAVE_SRC, "--ui-keys", "32,32,79", "--shot", REF_SHOT],
        check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    b = load(REF_SHOT)
    ok = a.shape == b.shape and bool((a == b).all())
    print("port_ui_town_test: %s town exit -> wilderness play" % ("ok" if ok else "FAIL"))
    return ok


def main():
    if not os.path.exists(SAVE_SRC):
        print("port_ui_town_test: SKIP (no %s)" % SAVE_SRC)
        return 0
    make_town_save()
    P = "32,32,79"          # space, space -> menu; 'O' opens the in-town save
    rc = 0
    # TOWN hub, then each building, then the leave-town amulet page, then a
    # round-trip back to town.
    tests = [
        ("town",            P,               "--town",    True),
        ("emporium",        P + ",69",       "--emporium", True),
        ("bank",            P + ",66",       "--bank",    True),
        ("house",           P + ",72",       "--house",   True),
        ("temple",          P + ",84",       "--temple",  True),
        ("amulet (leave)",  P + ",67",       "--amulet",  True),
        ("emporium->town",  P + ",69,32",    "--town",    True),
    ]
    for name, keys, flag, ws in tests:
        if not check(name, keys, flag, ws):
            rc = 1
    if not check_town_exit():
        rc = 1
    return rc


if __name__ == "__main__":
    sys.exit(main())
