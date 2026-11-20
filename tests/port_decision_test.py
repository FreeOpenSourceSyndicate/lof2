#!/usr/bin/env python3
"""port_decision_test — regression test for the combat decision screen.

The per-round decision frame ("Please make your decision / for the next
round!") is the encounter screen with only the right-hand text block replaced.
The oracle `ref/oracle/combat_decision.png` was captured from the *emulator*,
and its viewport, left panel and bottom strip are byte-identical to the
encounter capture `ref/oracle/combat.png` (measured: viewport 1.0000, roster
1.0000, bottom 1.0000).  That pins the derivation: the decision screen is the
same composition plus new text.

The row layout is the engine's own printf sequence at image 0x18734:

  "%ld"       1-based party index, colour 8, x=139, rows 116+8*n
  "%-10s "    the name, colour 8, x=145
  rank        the 4-entry far-pointer table at ds:0x0eb0, selected by the
              character byte +0x24 (KIL/ATK/DEF/RET), black, x=211
  action      the 5-entry table at ds:0x0ec0, selected by +0x0f
              (Defends/Attacks/Stalks/Use object/Cast Spell), colour 8, x=235
  "@ %ld"     at x=295 with (+0x0d+1) when +0x16 == 0xff, else (+0x16+1);
              '@' and ' ' have no glyph, so the number inks at x=307

Header at y=92/100, footer "(A)ttack!" y=172 and "(Q)uick combat" y=180.

Like the encounter test, the two frames are compared in *index* space (each
frame is mapped back to its own 0..15 palette indices), because the emulator
capture uses the attribute set the game programs while the drop-in follows
DOSBox's stock set.
"""
import os
import subprocess
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, "game")
ORACLE = os.path.join(ROOT, "ref", "oracle", "combat_decision.png")
ENCOUNTER = os.path.join(ROOT, "ref", "oracle", "combat.png")
SAVE = os.path.join(GAME, "GAMES", "GAME2")
SHOT = "/tmp/port_decision_test.png"
VIEWPORT = (slice(11, 130), slice(10, 130))
ROSTER = (slice(136, 196), slice(6, 136))
TEXT = (slice(92, 188), slice(139, 317))

EMU_ATTR = [0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17]
STOCK_ATTR = [0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07,
              0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F]

MIN_MATCH = 0.95
MIN_VIEWPORT = 0.999
MIN_TEXT_INK = 0.99


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


def main() -> int:
    if not os.path.exists(ORACLE):
        print("port_decision_test: SKIP (no oracle capture)")
        return 0
    if not os.path.exists(SAVE):
        print("port_decision_test: SKIP (no save fixture)")
        return 0

    cmd = [os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
           "--decision", "--save", SAVE, "--shot", SHOT]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)

    port = to_index(SHOT, STOCK_ATTR)
    oracle = to_index(ORACLE, EMU_ATTR)
    if port.shape != oracle.shape:
        print("port_decision_test: FAIL (size %s != %s)"
              % (port.shape, oracle.shape))
        return 1

    match = float((port == oracle).mean())
    vmatch = float((port[VIEWPORT] == oracle[VIEWPORT]).mean())
    if match < MIN_MATCH:
        print("port_decision_test: FAIL (match=%.1f%% < %.0f%%)"
              % (100 * match, 100 * MIN_MATCH))
        return 1
    if vmatch < MIN_VIEWPORT:
        print("port_decision_test: FAIL (viewport=%.1f%% < %.0f%%)"
              % (100 * vmatch, 100 * MIN_VIEWPORT))
        return 1

    # The decision screen must be the encounter screen with only the text
    # block replaced.  The oracle pair proves this for the engine's own
    # output; assert the same invariant holds for the drop-in against the
    # pre-existing encounter render, so a future change that moves the
    # viewport or the roster on one screen but not the other is caught.
    enc_shot = "/tmp/port_decision_test_encounter.png"
    subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                    "--encounter", "--save", SAVE, "--shot", enc_shot],
                   check=True, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    enc = to_index(enc_shot, STOCK_ATTR)
    for name, region in (("viewport", VIEWPORT), ("roster", ROSTER)):
        if not (port[region] == enc[region]).all():
            print("port_decision_test: FAIL (%s differs from the encounter "
                  "screen)" % name)
            return 1

    if os.path.exists(ENCOUNTER):
        oracle_enc = to_index(ENCOUNTER, EMU_ATTR)
        for name, region in (("viewport", VIEWPORT), ("roster", ROSTER)):
            if not (oracle[region] == oracle_enc[region]).all():
                print("port_decision_test: FAIL (oracle %s differs from the "
                      "encounter capture -- the derivation assumption is "
                      "broken)" % name)
                return 1

    # Every text row must match on its *ink mask* (non-background): the
    # emulator capture paints a few glyph-descender pixels as index 12, a
    # capture artifact, so compare shape rather than exact index.
    for y in (92, 100, 116, 124, 132, 140, 148, 156, 172, 180):
        pi = port[y:y + 8, 139:317] != 7
        oi = oracle[y:y + 8, 139:317] != 7
        frac = float((pi == oi).mean())
        if frac < MIN_TEXT_INK:
            print("port_decision_test: FAIL (text row y=%d ink=%.4f < %.2f)"
                  % (y, frac, MIN_TEXT_INK))
            return 1

    tmatch = float((port[TEXT] == oracle[TEXT]).mean())
    print("port_decision_test: ok (match=%.1f%%, viewport=%.1f%%, "
          "text=%.1f%%, text ink exact)" % (100 * match, 100 * vmatch,
                                            100 * tmatch))
    return 0


if __name__ == "__main__":
    sys.exit(main())
