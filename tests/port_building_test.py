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
import re
import subprocess
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, "game")
GAME4 = os.path.join(GAME, "GAMES", "GAME4")
# Committed fixture: character 1 dead (status 7, 0 HP), character 2 has 2000
# gold — the setup the owner described for testing the Temple revive.
REVIVE_FIXTURE = os.path.join(ROOT, "saves", "dng00-Conan-393da5b1.sav")

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


def check_temple_revive_effect() -> int:
    # --temple-revive-do N on the committed fixture (dead char 1 + rich char 2):
    # cost level*300; roll 1 <= CON*4+5 -> status 0, hp_cur>=1, CON--.
    if not os.path.exists(REVIVE_FIXTURE):
        print("port_building_test: SKIP temple-revive effect (no fixture)")
        return 0

    def rec(b, i):
        return b[4 + i * 410:4 + (i + 1) * 410]

    def gold(r):
        return r[0x72] | (r[0x72 + 1] << 8) | (r[0x72 + 2] << 16) | (r[0x72 + 3] << 24)

    a = open(REVIVE_FIXTURE, "rb").read()
    r0 = rec(a, 0)
    if r0[0x1f] != 7:
        print("port_building_test: SKIP temple-revive effect (fixture char1 alive)")
        return 0
    lvl, con, cost = r0[0x17], r0[0x44], r0[0x17] * 300
    out = "/tmp/port_temple_revived.sav"
    r = subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                        "--temple-revive-do", "1", "--temple-revive-payer", "2",
                        "--temple-revive-roll", "1", "--save", REVIVE_FIXTURE,
                        "--amulet-out", out],
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    if r.returncode != 0:
        print("port_building_test: FAIL temple-revive effect (%s)"
              % r.stderr.decode().strip())
        return 1
    b = open(out, "rb").read()
    r0b = rec(b, 0)
    if r0b[0x1f] != 0:
        print("port_building_test: FAIL temple-revive effect (status not cleared)")
        return 1
    hp = r0b[0x22] | (r0b[0x23] << 8)
    if hp < 1:
        print("port_building_test: FAIL temple-revive effect (hp_cur %d < 1)" % hp)
        return 1
    if r0b[0x44] != con - 1:
        print("port_building_test: FAIL temple-revive effect (CON %d != %d)"
              % (r0b[0x44], con - 1))
        return 1
    if gold(rec(a, 1)) - gold(rec(b, 1)) != cost:
        print("port_building_test: FAIL temple-revive effect (paid %d != %d)"
              % (gold(rec(a, 1)) - gold(rec(b, 1)), cost))
        return 1
    if len(b) != 13134 or b[0x2e3a:0x2e3e] == a[0x2e3a:0x2e3e]:
        print("port_building_test: FAIL temple-revive effect (save not resealed)")
        return 1
    print("port_building_test: ok temple-revive effect (fixture: status->alive, "
          "hp>=1, CON--, paid %dg, resealed)" % cost)
    return 0


def check_bank_effect() -> int:
    # --bank-deposit N AMT moves gold(+0x72) -> account(+0x76); --bank-withdraw
    # N AMT moves it back (image 0xbb7a..0xbf7e).
    if not os.path.exists(GAME4):
        print("port_building_test: SKIP bank effect (no GAME4)")
        return 0

    def rec(b, i):
        return b[4 + i * 410:4 + (i + 1) * 410]

    def dword(r, off):
        return r[off] | (r[off + 1] << 8) | (r[off + 2] << 16) | (r[off + 3] << 24)

    a = open(GAME4, "rb").read()
    if dword(rec(a, 0), 0x72) < 5:
        print("port_building_test: SKIP bank effect (char1 gold < 5)")
        return 0
    d1, d2 = "/tmp/port_bank_dep.sav", "/tmp/port_bank_wd.sav"
    r = subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                        "--bank-deposit", "1", "5", "--save", GAME4,
                        "--amulet-out", d1],
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    if r.returncode != 0:
        print("port_building_test: FAIL bank effect (deposit: %s)"
              % r.stderr.decode().strip())
        return 1
    b = open(d1, "rb").read()
    if dword(rec(b, 0), 0x72) != dword(rec(a, 0), 0x72) - 5 or \
       dword(rec(b, 0), 0x76) != dword(rec(a, 0), 0x76) + 5:
        print("port_building_test: FAIL bank effect (deposit moved wrong amount)")
        return 1
    r = subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                        "--bank-withdraw", "1", "5", "--save", d1,
                        "--amulet-out", d2],
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    if r.returncode != 0:
        print("port_building_test: FAIL bank effect (withdraw: %s)"
              % r.stderr.decode().strip())
        return 1
    c = open(d2, "rb").read()
    if dword(rec(c, 0), 0x72) != dword(rec(a, 0), 0x72) or \
       dword(rec(c, 0), 0x76) != dword(rec(a, 0), 0x76):
        print("port_building_test: FAIL bank effect (withdraw not restoring)")
        return 1
    if len(b) != 13134 or b[0x2e3a:0x2e3e] == a[0x2e3a:0x2e3e]:
        print("port_building_test: FAIL bank effect (save not resealed)")
        return 1
    print("port_building_test: ok bank effect (deposit/withdraw round-trip, "
          "resealed)")
    return 0


def check_joint_deposit_effect() -> int:
    # --joint-deposit N pools every present member's gold into the recipient
    # (shared routine image 0x28caa; also the emporium "(G)ather gold").
    if not os.path.exists(GAME4):
        print("port_building_test: SKIP joint-deposit effect (no GAME4)")
        return 0

    def rec(b, i):
        return b[4 + i * 410:4 + (i + 1) * 410]

    def gold(r):
        return r[0x72] | (r[0x72 + 1] << 8) | (r[0x72 + 2] << 16) | (r[0x72 + 3] << 24)

    a = open(GAME4, "rb").read()
    total = sum(gold(rec(a, i)) for i in range(6) if rec(a, i)[0])
    if total == 0:
        print("port_building_test: SKIP joint-deposit effect (no gold)")
        return 0
    out = "/tmp/port_joint.sav"
    r = subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                        "--joint-deposit", "1", "--save", GAME4, "--amulet-out", out],
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    if r.returncode != 0:
        print("port_building_test: FAIL joint-deposit effect (%s)"
              % r.stderr.decode().strip())
        return 1
    b = open(out, "rb").read()
    if gold(rec(b, 0)) != total:
        print("port_building_test: FAIL joint-deposit effect (target %d != %d)"
              % (gold(rec(b, 0)), total))
        return 1
    for i in range(1, 6):
        if rec(b, i)[0] and gold(rec(b, i)) != 0:
            print("port_building_test: FAIL joint-deposit effect (char %d still has %d)"
                  % (i + 1, gold(rec(b, i))))
            return 1
    if len(b) != 13134 or b[0x2e3a:0x2e3e] == a[0x2e3a:0x2e3e]:
        print("port_building_test: FAIL joint-deposit effect (save not resealed)")
        return 1
    print("port_building_test: ok joint-deposit effect (%dg pooled into char 1, "
          "resealed)" % total)
    return 0


def check_buy_effect() -> int:
    # --buy N ITEM: pay item.price, add the item, inv_used++.  Rich fixture.
    if not os.path.exists(REVIVE_FIXTURE):
        print("port_building_test: SKIP buy effect (no fixture)")
        return 0

    def rec(b, i):
        return b[4 + i * 410:4 + (i + 1) * 410]

    def gold(r):
        return r[0x72] | (r[0x72 + 1] << 8) | (r[0x72 + 2] << 16) | (r[0x72 + 3] << 24)

    item = 3  # "Hand axe": cheap, carried by the default shop
    a = open(REVIVE_FIXTURE, "rb").read()
    inv_before = [rec(a, 1)[0x82 + i * 4] for i in range(48)]
    if sum(1 for v in inv_before if v == 0) == 0:
        print("port_building_test: SKIP buy effect (inventory full)")
        return 0
    out = "/tmp/port_buy.sav"
    r = subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                        "--buy", "2", str(item), "--save", REVIVE_FIXTURE,
                        "--amulet-out", out],
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    if r.returncode != 0:
        print("port_building_test: FAIL buy effect (%s)"
              % r.stderr.decode().strip())
        return 1
    m = re.search(r"for (\d+) gold", r.stderr.decode())
    if not m:
        print("port_building_test: FAIL buy effect (no price in output)")
        return 1
    price = int(m.group(1))
    b = open(out, "rb").read()
    rb = rec(b, 1)
    if gold(rec(a, 1)) - gold(rb) != price:
        print("port_building_test: FAIL buy effect (gold %d->%d, price %d)"
              % (gold(rec(a, 1)), gold(rb), price))
        return 1
    inv_after = [rb[0x82 + i * 4] for i in range(48)]
    if inv_after.count(item) != inv_before.count(item) + 1:
        print("port_building_test: FAIL buy effect (item %d not added)" % item)
        return 1
    if rb[0x6b] != rec(a, 1)[0x6b] + 1:
        print("port_building_test: FAIL buy effect (inv_used %d->%d)"
              % (rec(a, 1)[0x6b], rb[0x6b]))
        return 1
    if len(b) != 13134 or b[0x2e3a:0x2e3e] == a[0x2e3a:0x2e3e]:
        print("port_building_test: FAIL buy effect (save not resealed)")
        return 1
    print("port_building_test: ok buy effect (item %d for %dg, inv_used+1, "
          "resealed)" % (item, price))
    return 0


def check_use_effect() -> int:
    # --use N ITEM: effect 10 (healing potion) raises HP, clamped to hp_max.
    if not os.path.exists(GAME4):
        print("port_building_test: SKIP use effect (no GAME4)")
        return 0

    def rec(b, i):
        return b[4 + i * 410:4 + (i + 1) * 410]

    a = open(GAME4, "rb").read()
    r0 = rec(a, 0)
    hp0 = r0[0x22] | (r0[0x22 + 1] << 8)
    mx0 = r0[0x20] | (r0[0x20 + 1] << 8)
    if hp0 >= mx0:
        print("port_building_test: SKIP use effect (char 1 not wounded)")
        return 0
    out = "/tmp/port_use.sav"
    r = subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                        "--use", "1", "47", "--save", GAME4, "--amulet-out", out],
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    if r.returncode != 0:
        print("port_building_test: FAIL use effect (%s)"
              % r.stderr.decode().strip())
        return 1
    b = open(out, "rb").read()
    r1 = rec(b, 0)
    hp1 = r1[0x22] | (r1[0x22 + 1] << 8)
    if hp1 <= hp0 or hp1 > mx0:
        print("port_building_test: FAIL use effect (hp %d -> %d, max %d)"
              % (hp0, hp1, mx0))
        return 1
    if len(b) != 13134 or b[0x2e3a:0x2e3e] == a[0x2e3a:0x2e3e]:
        print("port_building_test: FAIL use effect (save not resealed)")
        return 1
    print("port_building_test: ok use effect (healing potion, hp %d -> %d, "
          "resealed)" % (hp0, hp1))
    return 0


def check_cast_effect() -> int:
    # --cast N SPELL applies a spell's effect (apply_spell_effect): effect 14
    # (spell 25) sets target +0x12 = 6; effect 15 (spell 80, Healing I) heals.
    if not os.path.exists(GAME4):
        print("port_building_test: SKIP cast effect (no GAME4)")
        return 0

    def rec(b, i):
        return b[4 + i * 410:4 + (i + 1) * 410]

    out = "/tmp/port_cast.sav"
    r = subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                        "--cast", "1", "25", "--save", GAME4, "--amulet-out", out],
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    if r.returncode != 0:
        print("port_building_test: FAIL cast effect (%s)"
              % r.stderr.decode().strip())
        return 1
    r1 = rec(open(out, "rb").read(), 0)
    v = r1[0x12] | (r1[0x13] << 8)
    if v != 6:
        print("port_building_test: FAIL cast effect (effect 14: +0x12=%d, want 6)" % v)
        return 1
    a = open(GAME4, "rb").read()
    hp0 = rec(a, 0)[0x22] | (rec(a, 0)[0x23] << 8)
    mx0 = rec(a, 0)[0x20] | (rec(a, 0)[0x21] << 8)
    r = subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                        "--cast", "1", "80", "--save", GAME4, "--amulet-out", out],
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    if r.returncode != 0:
        print("port_building_test: FAIL cast heal (%s)"
              % r.stderr.decode().strip())
        return 1
    hp1 = rec(open(out, "rb").read(), 0)[0x22] | (rec(open(out, "rb").read(), 0)[0x23] << 8)
    if hp1 < hp0 or hp1 > mx0:
        print("port_building_test: FAIL cast heal (hp %d -> %d, max %d)"
              % (hp0, hp1, mx0))
        return 1
    print("port_building_test: ok cast effect (effect 14 -> +0x12=6; Healing I "
          "hp %d -> %d)" % (hp0, hp1))
    return 0


def check_building_party_panel() -> int:
    """The building screens show the shared 3x2 party grid in the top-right.
    Build a synthetic save whose slot 1 matches the house oracle's party
    (Stiletto / Warrior / 10% / Good; slots 2-6 empty), render --house with it
    and require the whole screen to be pixel-exact against house.png."""
    game2 = os.path.join(GAME, "GAMES", "GAME2")
    oracle = os.path.join(ROOT, "ref", "oracle", "house.png")
    if not (os.path.exists(game2) and os.path.exists(oracle)):
        print("port_building_test: SKIP party grid (no GAME2/house.png)")
        return 0

    d = bytearray(open(game2, "rb").read())

    def rec(i):
        return 4 + i * 410

    for i in range(1, 6):
        d[rec(i)] = 0                       # slots 2-6 empty
    r = rec(0)
    d[r + 0] = 1                            # slot 1 present
    nm = b"Stiletto"
    d[r + 1:r + 1 + 14] = nm + b"\x00" * (14 - len(nm))
    d[r + 0x1b] = 0                         # profession Warrior
    d[r + 0x1e] = 10                        # panel stat 10%
    d[r + 0x1f] = 0                         # status Good
    sav = "/tmp/port_building_stiletto.sav"
    open(sav, "wb").write(d)

    shot = "/tmp/port_building_panel.png"
    subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                    "--house", "--save", sav, "--cursor", "0,0", "--shot", shot],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    port = np.asarray(Image.open(shot).convert("RGB")).astype(int)
    ref = np.asarray(Image.open(oracle).convert("RGB")).astype(int)
    if port.shape != ref.shape:
        print("port_building_test: FAIL party grid (size %s != %s)"
              % (port.shape, ref.shape))
        return 1
    diff = int((port != ref).any(2).sum())
    if diff:
        print("port_building_test: FAIL party grid (%d px differ)" % diff)
        return 1
    print("port_building_test: ok party grid (house.png pixel-exact)")
    return 0


def main() -> int:
    rc = 0
    for flag, ref, label in BUILDINGS:
        rc |= check(flag, ref, label)
    rc |= check_building_party_panel()
    rc |= check_temple_heal_effect()
    rc |= check_temple_revive_effect()
    rc |= check_bank_effect()
    rc |= check_joint_deposit_effect()
    rc |= check_buy_effect()
    rc |= check_use_effect()
    rc |= check_cast_effect()
    return rc


if __name__ == "__main__":
    sys.exit(main())
