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
import re
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


def check_round_trip() -> int:
    """The round's HP must persist into the next round (write-back into the save).

    Two rounds against tough monsters (2x200 HP) so the defenders survive and
    hit; the combat-start HP of round 2 must equal the round-end HP of round 1,
    and at least one member must actually lose HP in round 1 (so the check bites).
    """
    game2 = os.path.join(GAME, "GAMES", "GAME2")
    if not os.path.exists(game2):
        print("port_ui_test: SKIP round-trip (no GAME2)")
        return 0
    r = subprocess.run(
        [os.path.join(ROOT, "lof"), "--dir", GAME, "--headless", "--ui",
         "--ui-keys", "32,32,79,67,13,13,67,13,81", "--ui-encounter", "2:200",
         "--save", game2, "--shot", SHOT],
        stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    err = r.stderr.decode()
    starts, ends = [], []
    for line in err.splitlines():
        if "combat start:" in line:
            starts.append(line.split("combat start:", 1)[1].strip())
        elif "round end:" in line:
            ends.append(line.split("round end:", 1)[1].strip())
    n = len(starts) // 2
    if n == 0 or len(starts) != len(ends) or len(starts) % 2:
        print("port_ui_test: FAIL round-trip (%d starts, %d ends)" % (len(starts), len(ends)))
        return 1
    for i in range(n):
        if ends[i] != starts[n + i]:
            print("port_ui_test: FAIL round-trip (%s -> %s)" % (ends[i], starts[n + i]))
            return 1
    if starts[:n] == ends[:n]:
        print("port_ui_test: FAIL round-trip (no HP/XP change in round 1)")
        return 1
    # XP must accumulate (party damage -> experience), image 0x14D80.
    def exp_of(s):
        m = re.search(r"exp (\d+)", s)
        return int(m.group(1)) if m else 0
    if all(exp_of(ends[i]) == exp_of(starts[i]) for i in range(n)):
        print("port_ui_test: FAIL round-trip (no XP gained)")
        return 1
    print("port_ui_test: round-trip ok (round-2 start == round-1 end; HP lost, XP gained)")
    return 0


def check_cast_charges() -> int:
    """Casting consumes the memorised-spell charge (+0x143), decremented per cast.

    Two cast rounds by Lara (GAME4, Healing I, 10 charges) must show 10->9 then
    9->8 (image 0x12A1E decrements +0x143).
    """
    game4 = os.path.join(GAME, "GAMES", "GAME4")
    if not os.path.exists(game4):
        print("port_ui_test: SKIP cast-charges (no GAME4)")
        return 0
    r = subprocess.run(
        [os.path.join(ROOT, "lof"), "--dir", GAME, "--headless", "--ui",
         "--ui-keys", "32,32,79,67,51,13,13,67,51,13,81", "--save", game4,
         "--shot", SHOT],
        stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    err = r.stderr.decode()
    if "charges 10 -> 9" not in err or "charges 9 -> 8" not in err:
        print("port_ui_test: FAIL cast-charges (expected 10->9 then 9->8)")
        return 1
    print("port_ui_test: cast-charges ok (Healing I 10->9->8)")
    return 0


def check_learn_magic() -> int:
    """The --ui House (L) learn flow: pick char -> gate -> fee -> payer -> teach.

    Self-contained: build a town save (location 8, no DNG08) with a caster that
    has learned 0, item 40 (spellbook) and 1000 gold, then script
    [space,space,O,H,L,<char>,<payer>] and require the "learns N spell(s) for C
    gold ... Concentration improves!" log (image 0x89da / 0x9B7C).
    """
    school_of = {3: 5, 5: 3, 6: 2, 7: 0, 8: 1, 10: 4}
    src = None
    for cand in (os.path.join(GAME, "GAMES", "GAME4"),
                 os.path.join(GAME, "GAMES", "GAME2")):
        if os.path.exists(cand):
            src = cand
            break
    if src is None:
        print("port_ui_test: SKIP learn-magic (no save)")
        return 0
    a = bytearray(open(src, "rb").read())
    idx = None
    for i in range(6):
        r = a[4 + i * 410:4 + (i + 1) * 410]
        if r[0] and r[0x1b] in school_of:
            idx = i
            break
    if idx is None:
        print("port_ui_test: SKIP learn-magic (no caster)")
        return 0
    base = 4 + idx * 410
    a[base + 0x6a] = 0                                    # learned 0
    for k in range(4):
        a[base + 0x72 + k] = (1000 >> (8 * k)) & 0xff     # 1000 gold
    a[base + 0x82] = 40                                   # item 40 (spellbook)
    if a[base + 0x6b] < 1:
        a[base + 0x6b] = 1
    a[0x9a2] = 8
    a[0x9a4] = 0                                          # location 8 -> town
    KS = 0x2e3a
    acc = 1
    for i in range(KS):
        c = a[i] if a[i] else 1
        acc = ((acc + c) if i % 2 == 0 else (acc ^ c)) & 0xFFFFFFFF
    for i in range(4):
        a[KS + i] = (acc >> (8 * i)) & 0xff
    save = "/tmp/ui_learn_in.sav"
    open(save, "wb").write(a)
    d = 49 + idx   # ASCII of the 1-based slot digit ('1'=49 .. '6'=54)
    keys = "32,32,79,72,76,%d,%d" % (d, d)   # space,space,O,H,L,char,payer
    r = subprocess.run(
        [os.path.join(ROOT, "lof"), "--dir", GAME, "--headless", "--ui",
         "--ui-keys", keys, "--save", save, "--shot", SHOT],
        stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    err = r.stderr.decode()
    if "learns" not in err or "Concentration improves!" not in err:
        print("port_ui_test: FAIL learn-magic (%r)" % err[-200:])
        return 1
    line = [l for l in err.splitlines() if "learns" in l][0]
    print("port_ui_test: learn-magic ok (%s)" % line.strip())
    return 0


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

    # Cast path: from play, 'C' opens the combat decision, a digit selects a
    # caster (GAME4's Lara, Healing I), Enter commits.  The decision log confirms
    # the queued cast.
    game4 = os.path.join(GAME, "GAMES", "GAME4")
    if os.path.exists(game4):
        r = subprocess.run(
            [os.path.join(ROOT, "lof"), "--dir", GAME, "--headless", "--ui",
             "--ui-keys", "32,32,79,67,51,13,81", "--save", game4,
             "--shot", SHOT],
            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        err = r.stderr.decode()
        if r.returncode != 0:
            print("port_ui_test: FAIL cast path (%s)" % err.strip())
            rc = 1
        elif "will cast Healing I" not in err:
            print("port_ui_test: FAIL cast path (no 'will cast Healing I')")
            rc = 1
        else:
            print("port_ui_test: cast ok (Lara casts Healing I at the decision)")
    else:
        print("port_ui_test: SKIP cast (no GAME4)")

    rc |= check_round_trip()
    rc |= check_cast_charges()
    rc |= check_learn_magic()

    return rc


if __name__ == "__main__":
    sys.exit(main())
