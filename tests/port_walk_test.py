#!/usr/bin/env python3
"""port_walk_test — regression test for the drop-in's native walk loop (`--walk`).

`--walk` is the front-end half of the in-game play loop: it loads the save's
dungeon (DNGS/DNG<d><l>, derived from the save's location byte), places the party
on the level and replays a scancode sequence through the walk model
(up 0x48 = step forward, 0x50 = turn 180, 0x4b = turn left, 0x4d = turn right),
then renders the play screen with the level map in the viewport.  It reads only
game data -- DNGS/ and GAMES/ -- and never executes an original binary.

The test drives the loop head-lessly and asserts the printed position against the
walk model's own contract.  The party spawns at **the cell the save stored**
(`0x9a0`/`0x9a6`, verified by patching: 125->455 moves the emulator viewport
from 1.0000 to 0.0380 vs `dungeon_slot1.png`), falling back to the level's entry
cell (the DNG header's `entry_pos`/`entry_facing`) only when the save has no
position word.  The two agree for the Temple save (`GAME1` was written at the
entry, cell 125 = (17,3) facing S), so that save's cases are unchanged.  From
there:

  * a turn left yields facing E (1) and a turn right yields W (3) (facing S);
  * a step is taken only when the cell ahead is passable, so the party stays put
    when the cell ahead is a wall (tile 6/13);
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

    # Spawn is the DNG05 entry cell (17,3) facing S (2).  Turning left -> E (1),
    # right -> W (3); turning 180 twice is the identity.
    expect("", (17, 3, 2), "spawn")
    expect("0x4b", (17, 3, 1), "turn left")
    expect("0x4d", (17, 3, 3), "turn right")
    expect("0x50,0x50", (17, 3, 2), "turn 180 twice")
    # Facing S at (17,3) steps into the open interior: two steps -> (17,5).
    expect("0x48,0x48", (17, 5, 2), "step south twice")
    # Turn 180 to face N (0), step to (17,2), then the next cell (17,1) is tile 3
    # (open) and (17,0) is tile 6 (a wall): the party stops at (17,1).
    expect("0x50,0x48,0x48,0x48", (17, 1, 0), "blocked at north wall")

    # A save taken away from the level entry must spawn at *its* cell, not at
    # the DNG header's.  dng05-Gonk stores cell 455 = (23,12) facing E (1) while
    # DNG05's header says 125 = (17,3) facing S (2), so this pins the
    # save-over-header precedence that the emulator was measured to obey.
    gonk = os.path.join(ROOT, "saves", "dng05-Gonk-b2b765be.sav")
    if os.path.exists(gonk):
        out = subprocess.run(
            [os.path.join(ROOT, "lof"), "--dir", GAME, "--headless", "--walk",
             "--save", gonk], check=True, capture_output=True, text=True).stdout
        m = re.search(r"walk: x=(-?\d+) y=(-?\d+) facing=(\d+)", out)
        got = tuple(int(g) for g in m.groups()) if m else None
        if got == (23, 12, 1):
            print("port_walk_test: save position ok (Gonk spawns at (23,12), "
                  "not the header's (17,3))")
        else:
            print("port_walk_test: FAIL save position (got %s, want (23, 12, 1))"
                  % (got,))
            rc = 1

    # --- Level transitions -------------------------------------------------
    # A stair is a tile code: low nibble 4 = up, 5 = down, high nibble = the
    # destination dungeon number; the destination cell is the cell's object
    # record, a big-endian row-major index.  DNG05's down-stair is cell 1211
    # (23,33), tile 0x15, and its record points at DNG15 cell 644 = (32,17).
    # Standing on the passable cell east of it (1212, tile 0) facing W and
    # stepping forward must therefore move the party into DNG15 at (32,17)
    # while keeping the facing.
    #
    # The save is a scratch copy of GAME1 with the party moved to that cell; the
    # shipped saves are left untouched.
    import shutil
    import tempfile
    stair_save = os.path.join(tempfile.mkdtemp(prefix="lof_stair_"), "GAME1")
    with open(SAVE, "rb") as f:
        data = bytearray(f.read())

    def put16(buf, off, val):
        buf[off] = val & 0xFF
        buf[off + 1] = (val >> 8) & 0xFF

    put16(data, 0x9A0, 1212)   # party cell (24,33)
    put16(data, 0x9A2, 5)      # level digit  5
    put16(data, 0x9A4, 0)      # dungeon digit 0  -> DNG05
    put16(data, 0x9A6, 3)      # facing W
    with open(stair_save, "wb") as f:
        f.write(data)

    cmd = [os.path.join(ROOT, "lof"), "--dir", GAME, "--headless", "--walk",
           "--save", stair_save, "--walk-keys", "0x48"]
    res = subprocess.run(cmd, check=True, capture_output=True, text=True)
    m = re.search(r"walk: x=(-?\d+) y=(-?\d+) facing=(\d+)", res.stdout)
    got = tuple(int(g) for g in m.groups()) if m else None
    if got == (32, 17, 3) and "You climb down the stairs." in res.stdout:
        print("port_walk_test: level transition ok (DNG05 cell 1211 -> DNG15 "
              "(32,17))")
    else:
        print("port_walk_test: FAIL level transition (got %s, want (32, 17, 3); "
              "output %r)" % (got, res.stdout))
        rc = 1
    shutil.rmtree(os.path.dirname(stair_save), ignore_errors=True)

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
