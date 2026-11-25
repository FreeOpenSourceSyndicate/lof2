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
GAME2 = os.path.join(GAME, "GAMES", "GAME2")
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

    # XP boost = (Con_after * prof_base(prof) * 2^(level-1)) / +0x60, with the
    # 16-bit product truncated and prof_base == the training-cost table.
    def exp(r):
        return r[0x6c] | (r[0x6d] << 8) | (r[0x6e] << 16) | (r[0x6f] << 24)

    def prof_base(p):
        return 1500 if p in (2, 3, 10, 11) else (2000 if p in (0, 1, 4, 9) else 2250)

    c60 = r0[0x60]
    shift = (1 << (lvl - 1)) if 1 <= lvl <= 16 else 0
    prod = (prof_base(r0[0x1b]) * shift) & 0xffff
    expected_boost = ((con - 1) * prod) // c60 if c60 else 0
    if exp(r0b) != exp(r0) + expected_boost:
        print("port_building_test: FAIL temple-revive effect (exp %d -> %d, "
              "want +%d)" % (exp(r0), exp(r0b), expected_boost))
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


def check_repair_effect() -> int:
    # --repair N SLOT: cost = price - condition*price/100, condition -> 100.
    # GAME2 char 1 slot 1 is a worn "Small shield" (condition 92).
    if not os.path.exists(GAME2):
        print("port_building_test: SKIP repair effect (no GAME2)")
        return 0

    def rec(b, i):
        return b[4 + i * 410:4 + (i + 1) * 410]

    def gold(r):
        return r[0x72] | (r[0x72 + 1] << 8) | (r[0x72 + 2] << 16) | (r[0x72 + 3] << 24)

    a = open(GAME2, "rb").read()
    slot = 1
    cond_before = rec(a, 0)[0x82 + slot * 4 + 3]
    if rec(a, 0)[0x82 + slot * 4] == 0 or cond_before >= 100:
        print("port_building_test: SKIP repair effect (slot %d not worn)" % slot)
        return 0
    out = "/tmp/port_repair.sav"
    r = subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                        "--repair", "1", str(slot), "--save", GAME2,
                        "--amulet-out", out],
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    if r.returncode != 0:
        print("port_building_test: FAIL repair effect (%s)"
              % r.stderr.decode().strip())
        return 1
    m = re.search(r"for (\d+) gold", r.stderr.decode())
    if not m:
        print("port_building_test: FAIL repair effect (no cost in output)")
        return 1
    cost = int(m.group(1))
    b = open(out, "rb").read()
    rb = rec(b, 0)
    if gold(rec(a, 0)) - gold(rb) != cost:
        print("port_building_test: FAIL repair effect (gold %d->%d, cost %d)"
              % (gold(rec(a, 0)), gold(rb), cost))
        return 1
    if rb[0x82 + slot * 4 + 3] != 100:
        print("port_building_test: FAIL repair effect (condition %d->%d)"
              % (cond_before, rb[0x82 + slot * 4 + 3]))
        return 1
    if len(b) != 13134:
        print("port_building_test: FAIL repair effect (save not resealed)")
        return 1
    print("port_building_test: ok repair effect (condition %d->100 for %dg, resealed)"
          % (cond_before, cost))
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


def check_training_effect() -> int:
    # --train N HP: level++, HP cur/max += hp, and the derived-stat recompute
    # (image 0x96F7..0x9901): magic max/cur from the profession x level table
    # plus the Wisdom/Intelligence bonuses, weight max from Strength, and base
    # Con = max(+0x60, Con).  GAME4 char 3 (Lara: prof 5, level 3, Wis 19,
    # Int 12, Str 12) -> level 4, magic max/cur 6, weight max 4200.
    if not os.path.exists(GAME4):
        print("port_building_test: SKIP training effect (no GAME4)")
        return 0

    def rec(b, i):
        return b[4 + i * 410:4 + (i + 1) * 410]

    a = open(GAME4, "rb").read()
    r0 = rec(a, 2)   # Lara (0-based index 2 == --train 3)
    if r0[0x1b] != 5 or r0[0x17] != 3 or r0[0x48] <= 15 or r0[0x47] > 15:
        print("port_building_test: SKIP training effect (Lara stats differ)")
        return 0
    out = "/tmp/port_trained.sav"
    r = subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                        "--train", "3", "5", "--save", GAME4,
                        "--amulet-out", out],
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    if r.returncode != 0:
        print("port_building_test: FAIL training effect (%s)"
              % r.stderr.decode().strip())
        return 1
    rb = rec(open(out, "rb").read(), 2)
    if rb[0x17] != 4:
        print("port_building_test: FAIL training effect (level %d != 4)" % rb[0x17])
        return 1
    if rb[0x68] != 6 or rb[0x69] != 6:
        print("port_building_test: FAIL training effect (magic %d/%d != 6/6)"
              % (rb[0x68], rb[0x69]))
        return 1
    if (rb[0x64] | (rb[0x65] << 8)) != 4200:
        print("port_building_test: FAIL training effect (weight max != 4200)")
        return 1
    if rb[0x1c] != 90:   # base stat: (level-1)=3 divides prof-5 base table 3
        print("port_building_test: FAIL training effect (base stat %d != 90)"
              % rb[0x1c])
        return 1
    # Speed nudge: Joanna (prof 10, level 4 -> 5; speed table 5, 5 % 5 == 0).
    r0j = rec(a, 4)
    if r0j[0x1b] == 10 and r0j[0x17] == 4 and r0j[0x56] < 0xff:
        outj = "/tmp/port_trained_j.sav"
        rj = subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                             "--train", "5", "5", "--save", GAME4,
                             "--amulet-out", outj],
                            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        if rj.returncode != 0:
            print("port_building_test: FAIL training effect (Joanna %s)"
                  % rj.stderr.decode().strip())
            return 1
        rbj = rec(open(outj, "rb").read(), 4)
        if rbj[0x56] != r0j[0x56] + 1:
            print("port_building_test: FAIL training effect (Joanna speed %d != %d)"
                  % (rbj[0x56], r0j[0x56] + 1))
            return 1
        if rbj[0x1c] != 90:
            print("port_building_test: FAIL training effect (Joanna base %d != 90)"
                  % rbj[0x1c])
            return 1
    print("port_building_test: ok training effect (Lara -> level 4, magic 6/6, "
          "weight max 4200, base 90; Joanna speed+1, base 90)")
    return 0


def check_house_train() -> int:
    # --house-train N PAYER HP: full House-of-Trades training — gate on the
    # accumulated experience (training_cost(prof) * 2^(level-1), not spent),
    # then charge the flat gold fee from the payer, then level up.  Exercised on
    # the real GAME4 fixture: an XP-short refusal, a gold-short refusal, and a
    # success (payer gold patched in a temp copy so the flat fee is affordable).
    if not os.path.exists(GAME4):
        print("port_building_test: SKIP house-train (no GAME4)")
        return 0
    COST = [2000, 2000, 1500, 1500, 2000, 2250, 2250, 2250, 2250, 2000, 1500, 1500]

    def rec(b, i):
        return b[4 + i * 410:4 + (i + 1) * 410]

    def dword(r, off):
        return r[off] | (r[off + 1] << 8) | (r[off + 2] << 16) | (r[off + 3] << 24)

    a = open(GAME4, "rb").read()
    c1 = rec(a, 0)   # Conan (prof 0, level 3): 7723 XP < 8000 -> XP short.
    prof, lvl = c1[0x1b], c1[0x17]
    xp = dword(c1, 0x6c)
    need = COST[prof] * (2 ** (lvl - 1)) if lvl >= 1 else 0
    if xp >= need:
        print("port_building_test: SKIP house-train XP-short (char 1 has XP)")
    else:
        r = subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                            "--house-train", "1", "1", "5", "--save", GAME4],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        out = r.stdout.decode()
        if ("needs %d more experience points" % (need - xp)) not in out:
            print("port_building_test: FAIL house-train XP short (%r)" % out.strip())
            return 1

    # Gold short: Lara (char 3) has enough XP but only 7 gold < her $ fee.
    c3 = rec(a, 2)
    prof3 = c3[0x1b]
    cost3 = COST[prof3]
    if dword(c3, 0x72) < cost3 and dword(c3, 0x6c) >= COST[prof3] * (2 ** (c3[0x17] - 1)):
        r = subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                            "--house-train", "3", "3", "5", "--save", GAME4],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        out = r.stdout.decode()
        if ("cannot afford %d gold" % cost3) not in out:
            print("port_building_test: FAIL house-train gold short (%r)" % out.strip())
            return 1

    # Success: patch Conan's gold to 5000 in a temp copy, train Lara with Conan
    # as payer -> level 3->4, Conan charged the flat fee.
    rich = bytearray(a)
    off = 4 + 0 * 410 + 0x72
    rich[off:off + 4] = (5000).to_bytes(4, "little")
    tmp_in = "/tmp/game4_rich.sav"
    open(tmp_in, "wb").write(rich)
    lara = rec(a, 2)
    if lara[0x1b] == 5 and lara[0x17] == 3 and dword(lara, 0x6c) >= COST[5] * 4:
        tmp_out = "/tmp/game4_trained.sav"
        r = subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                            "--house-train", "3", "1", "5", "--save", tmp_in,
                            "--amulet-out", tmp_out],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        out = r.stdout.decode()
        if "trained to level 4 for 2250 gold" not in out:
            print("port_building_test: FAIL house-train success (%r)" % out.strip())
            return 1
        b = open(tmp_out, "rb").read()
        if rec(b, 2)[0x17] != 4:
            print("port_building_test: FAIL house-train success (level %d)"
                  % rec(b, 2)[0x17])
            return 1
        if dword(rec(b, 0), 0x72) != 5000 - 2250:
            print("port_building_test: FAIL house-train success (payer gold %d)"
                  % dword(rec(b, 0), 0x72))
            return 1
    print("port_building_test: ok house-train (XP gate, gold fee, level-up)")
    return 0


def check_time_tick() -> int:
    # --time-tick N: the map tick (image 0x25BE2..0x25D30) — clock phase +1 wrap
    # >7->0, MP regen max(1,MPmax>>3) clamped, timer +0x57=4, NO HP change.
    if not os.path.exists(GAME4):
        print("port_building_test: SKIP time-tick (no GAME4)")
        return 0
    a = open(GAME4, "rb").read()
    clk0 = a[0x9b6] | (a[0x9b7] << 8) | (a[0x9b8] << 16) | (a[0x9b9] << 24)
    out = "/tmp/tick_out.sav"
    p = subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                        "--time-tick", "3", "--save", GAME4, "--amulet-out", out],
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if p.returncode != 0:
        print("port_building_test: FAIL time-tick rc=%d %r" % (p.returncode, p.stderr.decode()))
        return 1
    b = open(out, "rb").read()
    clk1 = b[0x9b6] | (b[0x9b7] << 8) | (b[0x9b8] << 16) | (b[0x9b9] << 24)
    if clk1 != (clk0 + 3) % 8:
        print("port_building_test: FAIL time-tick clock %d -> %d" % (clk0, clk1)); return 1
    # HP must be unchanged and +0x57 set to 4 for every present member.
    for i in range(6):
        ra = a[4 + i * 410:4 + (i + 1) * 410]
        rb = b[4 + i * 410:4 + (i + 1) * 410]
        if not ra[0]:
            continue
        if rb[0x57] != 4:
            print("port_building_test: FAIL time-tick +0x57=%d" % rb[0x57]); return 1
        if (ra[0x22] | (ra[0x23] << 8)) != (rb[0x22] | (rb[0x23] << 8)):
            print("port_building_test: FAIL time-tick HP changed"); return 1
    print("port_building_test: ok time-tick (clock+3 wrap, MP regen, no HP, +0x57=4)")
    return 0


def check_sell_mechanic() -> int:
    # --sell N SLOT PRICE: the deterministic sell mechanic — gold dword += price
    # then remove+compact the slot (image 0x611A + 0x212f:0x7b6e).  Verified
    # end-to-end on GAME4 (Gandalf, slot 0): gold +price, used-count -1, items
    # shift down.  (The live price is random — see the sell decode — so the CLI
    # takes it as a parameter.)
    if not os.path.exists(GAME4):
        print("port_building_test: SKIP sell (no GAME4)")
        return 0
    a = open(GAME4, "rb").read()
    # find a present character with at least 2 items
    idx = None
    for i in range(6):
        r = a[4 + i * 410:4 + (i + 1) * 410]
        if r[0] and r[0x6b] >= 2 and r[0x82] and r[0x82 + 4]:
            idx = i
            break
    if idx is None:
        print("port_building_test: SKIP sell (no suitable character)")
        return 0
    r = a[4 + idx * 410:4 + (idx + 1) * 410]
    gold0 = r[0x72] | (r[0x73] << 8) | (r[0x74] << 16) | (r[0x75] << 24)
    used0 = r[0x6b]
    old_item1 = r[0x82 + 4]
    price = 123
    out = "/tmp/sell_out.sav"
    p = subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                        "--sell", str(idx + 1), "0", str(price), "--save", GAME4,
                        "--amulet-out", out],
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if p.returncode != 0:
        print("port_building_test: FAIL sell rc=%d %r" % (p.returncode, p.stderr.decode()))
        return 1
    b = open(out, "rb").read()
    r2 = b[4 + idx * 410:4 + (idx + 1) * 410]
    gold1 = r2[0x72] | (r2[0x73] << 8) | (r2[0x74] << 16) | (r2[0x75] << 24)
    if gold1 != gold0 + price:
        print("port_building_test: FAIL sell gold %d -> %d" % (gold0, gold1)); return 1
    if r2[0x6b] != used0 - 1:
        print("port_building_test: FAIL sell used %d -> %d" % (used0, r2[0x6b])); return 1
    if r2[0x82] != old_item1:
        print("port_building_test: FAIL sell compaction (slot0 %d, want %d)"
              % (r2[0x82], old_item1)); return 1
    print("port_building_test: ok sell mechanic (gold +price, remove+compact)")
    return 0


def check_sell_auto() -> int:
    # --sell-auto N SLOT: computes the sell price via sell_price() (image
    # 0x5C24..0x5CA3: max(1, price*cond/100 - price/25)), weight-checks, then
    # applies gold+=price + remove+compact.  Verify gold rises by exactly the
    # printed price and the slot is removed.
    if not os.path.exists(GAME4):
        print("port_building_test: SKIP sell-auto (no GAME4)")
        return 0
    a = open(GAME4, "rb").read()
    idx = None
    for i in range(6):
        r = a[4 + i * 410:4 + (i + 1) * 410]
        if r[0] and r[0x6b] >= 2 and r[0x82] and r[0x82 + 4]:
            idx = i
            break
    if idx is None:
        print("port_building_test: SKIP sell-auto (no suitable character)")
        return 0
    r = a[4 + idx * 410:4 + (idx + 1) * 410]
    gold0 = r[0x72] | (r[0x73] << 8) | (r[0x74] << 16) | (r[0x75] << 24)
    used0 = r[0x6b]
    out = "/tmp/sell_auto_out.sav"
    p = subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                        "--sell-auto", str(idx + 1), "0", "--save", GAME4,
                        "--amulet-out", out],
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if p.returncode != 0:
        print("port_building_test: FAIL sell-auto rc=%d %r" % (p.returncode, p.stderr.decode()))
        return 1
    import re
    m = re.search(rb"for (\d+) gold", p.stdout)
    if not m:
        print("port_building_test: FAIL sell-auto no price in output %r" % p.stdout)
        return 1
    price = int(m.group(1))
    if price < 1:
        print("port_building_test: FAIL sell-auto price %d" % price); return 1
    b = open(out, "rb").read()
    r2 = b[4 + idx * 410:4 + (idx + 1) * 410]
    gold1 = r2[0x72] | (r2[0x73] << 8) | (r2[0x74] << 16) | (r2[0x75] << 24)
    if gold1 != gold0 + price:
        print("port_building_test: FAIL sell-auto gold %d -> %d (price %d)" % (gold0, gold1, price)); return 1
    if r2[0x6b] != used0 - 1:
        print("port_building_test: FAIL sell-auto used %d -> %d" % (used0, r2[0x6b])); return 1
    print("port_building_test: ok sell-auto (price %d computed, gold+price, removed)" % price)
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
    rc |= check_repair_effect()
    rc |= check_training_effect()
    rc |= check_house_train()
    rc |= check_sell_mechanic()
    rc |= check_sell_auto()
    rc |= check_time_tick()
    rc |= check_use_effect()
    rc |= check_cast_effect()
    return rc


if __name__ == "__main__":
    sys.exit(main())
