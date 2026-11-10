#!/usr/bin/env python3
"""port_ui_test — regression test for the drop-in's interactive front-end.

The `--ui` mode runs the front-end as a keyboard-driven state machine instead of
a fixed slideshow: it starts on the title screen, advances title -> credits ->
menu on any key, and at the menu the recovered hot-keys apply ('O' opens the
default save and switches to the in-game play screen, 'Q'/Esc quits).  This test
drives that machine head-lessly with a scripted key sequence (`--ui-keys`) and
checks the resulting frame against the same verified captures the other port
tests use, so a regression in the transitions is caught without a display.

  * space,space            -> advances to the menu (byte-identical to the direct
                              --shot menu, which port_menu_test checks against
                              the oracle)
  * space,space,'O'(79)    -> menu then open-file -> play screen (== play golden)

Run via `make golden-port` (or directly).  Needs PIL + numpy and the oracle PNG.
"""
import os
import subprocess
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, "game")
MENU_DIRECT = "/tmp/port_ui_test_menu.png"
PLAY_ORACLE = os.path.join(ROOT, "ref", "oracle", "play.png")
SHOT = "/tmp/port_ui_test.png"

# The scripted menu frame must be byte-identical to the direct menu composition
# (the transition wiring must not perturb the rendered frame).  The scripted
# play frame carries the same dynamic viewport overlay residue as
# port_play_test, so it uses that tolerance against the oracle.
PLAY_MIN_MATCH = 0.94


def run(keys):
    subprocess.run(
        [os.path.join(ROOT, "lof"), "--dir", GAME, "--headless", "--ui",
         "--ui-keys", keys, "--shot", SHOT],
        check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def load(path):
    return np.asarray(Image.open(path).convert("RGB")).astype(int)


def main() -> int:
    if not os.path.exists(PLAY_ORACLE):
        print("port_ui_test: SKIP (no oracle capture)")
        return 0

    # Reference: the menu rendered directly (no UI loop).
    subprocess.run(
        [os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
         "--shot", MENU_DIRECT],
        check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    rc = 0
    run("32,32")  # space, space -> title, credits, menu
    if (load(SHOT) == load(MENU_DIRECT)).all():
        print("port_ui_test: menu ok (identical to direct composition)")
    else:
        print("port_ui_test: FAIL menu (differs from direct composition)")
        rc = 1

    run("32,32,79")  # space, space, 'O' -> menu then open-file -> play screen
    port, oracle = load(SHOT), load(PLAY_ORACLE)
    p = float((port == oracle).all(2).mean()) if port.shape == oracle.shape else -1.0
    if p >= PLAY_MIN_MATCH:
        print("port_ui_test: play ok (match=%.1f%%)" % (100 * p))
    else:
        print("port_ui_test: FAIL play (match=%.1f%% < %.0f%%)" % (100 * p, 100 * PLAY_MIN_MATCH))
        rc = 1

    return rc


if __name__ == "__main__":
    sys.exit(main())
