#!/usr/bin/env python3
"""port_walk_test — regression test for the drop-in's native walk loop (`--walk`).

`--walk` is the front-end half of the in-game play loop: it loads the save's
dungeon (DNGS/DNG<d><l>, derived from the save's location byte), places the party
on the level and replays a scancode sequence through the walk model
(up 0x48 = step forward, 0x50 = turn 180, 0x4b = turn left, 0x4d = turn right),
then renders the play screen with the level map in the viewport.  It reads only
game data -- DNGS/ and GAMES/ -- and never executes an original binary.

The test drives the loop head-lessly and asserts the printed position against the
walk model's own contract:

  * from the spawn at (0,0) facing N (0), a turn left yields facing 3 and a turn
    right yields facing 1;
  * a step is taken only when the cell ahead is passable, so the party stays put
    when it faces the level edge (y-1 < 0 at (0,0) facing N);
  * turning 180 twice is a no-op on facing.

It also checks that the walk screen differs from the static `--play` screen, i.e.
the viewport really carries the rendered level rather than the plain tile.

Run via `make golden-port` (or directly).  Needs the real game data under game/.
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, "game")
SAVE = os.path.join(GAME, "GAMES", "GAME1")


def walk(keys, shot=None):
    cmd = [os.path.join(ROOT, "lof"), "--dir", GAME, "--headless", "--walk",
           "--save", SAVE]
    if keys:
        cmd += ["--walk-keys", keys]
    if shot:
        cmd += ["--shot", shot]
    out = subprocess.run(cmd, check=True, capture_output=True, text=True).stdout
    m = re.search(r"walk: x=(-?\d+) y=(-?\d+) facing=(\d+)", out)
    if not m:
        raise RuntimeError("no position in output: %r" % out)
    return tuple(int(g) for g in m.groups())


def main() -> int:
    if not os.path.exists(SAVE):
        print("port_walk_test: SKIP (no game data)")
        return 0

    rc = 0

    def expect(keys, want, label):
        nonlocal rc
        got = walk(keys)
        if got == want:
            print("port_walk_test: %s ok (x=%d y=%d facing=%d)" % ((label,) + got))
        else:
            print("port_walk_test: FAIL %s (got %s, want %s)" % (label, got, want))
            rc = 1

    # Spawn: (0,0) facing N.  Turning left -> W (3), right -> E (1).
    expect("", (0, 0, 0), "spawn")
    expect("0x4b", (0, 0, 3), "turn left")
    expect("0x4d", (0, 0, 1), "turn right")
    # 180 twice is the identity.
    expect("0x50,0x50", (0, 0, 0), "turn 180 twice")
    # Facing N at the top edge cannot step (y-1 < 0): position unchanged.
    expect("0x48,0x48", (0, 0, 0), "blocked at north edge")
    # Facing S (2) steps freely into the level interior.
    expect("0x50,0x48,0x48", (0, 2, 2), "step south twice")

    # The walk viewport must differ from the static play composition.
    walk_shot = "/tmp/port_walk_test.png"
    play_shot = "/tmp/port_walk_play.png"
    walk("0x48", walk_shot)
    subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                    "--play", "D5/DP001", "--save", SAVE, "--shot", play_shot],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        import numpy as np
        from PIL import Image
        a = np.asarray(Image.open(walk_shot).convert("RGB"))
        b = np.asarray(Image.open(play_shot).convert("RGB"))
        if a.shape == b.shape and not (a == b).all():
            print("port_walk_test: viewport ok (walk screen differs from --play)")
        else:
            print("port_walk_test: FAIL viewport (walk screen identical to --play)")
            rc = 1

        # The interactive front-end (--ui) must drive the same walk model: after
        # opening the save ('O') the arrow keys move the party, so a scripted
        # 'O', down, up, up must land on the same frame as the direct --walk.
        ui_shot = "/tmp/port_walk_ui.png"
        subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                        "--ui", "--save", SAVE,
                        "--ui-keys", "32,32,79,0x40000051,0x40000052,0x40000052",
                        "--shot", ui_shot],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        direct_shot = "/tmp/port_walk_direct.png"
        walk("0x50,0x48,0x48", direct_shot)
        u = np.asarray(Image.open(ui_shot).convert("RGB"))
        d = np.asarray(Image.open(direct_shot).convert("RGB"))
        if u.shape == d.shape and (u == d).all():
            print("port_walk_test: ui walk ok (--ui arrows == direct --walk)")
        else:
            print("port_walk_test: FAIL ui walk (--ui arrows differ from --walk)")
            rc = 1
    except ImportError:
        pass

    return rc


if __name__ == "__main__":
    sys.exit(main())
