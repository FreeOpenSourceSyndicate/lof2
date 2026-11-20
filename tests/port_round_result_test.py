#!/usr/bin/env python3
"""port_round_result_test — regression test for the round-result screen.

The melee round table ("The result of this round:") is the encounter chrome
with the right-hand text block replaced by the table.  The oracle
`ref/oracle/round_result.png` was captured from the *emulator* with the
documented drive:

  LOF_KEYAT="50:0x18,100:0x03,150:0x50,200:0x48,250:0x2e,650:0x1e,
             780:0x02,900:0x20,1020:0x2e"

(O, slot 3, Down, Up, C -> wilderness -> encounter -> A -> char 1 -> D -> C).

Layout is the engine's own draw sequence at image 0x1867D..0x189F6:

  y=92    "The result of this round:"              colour 8
  y=108   "Name       HP   W P B Attack"           colour 0
  y=116   "---------- ---- - - - -------"          colour 0 (ink at y=119)
  y=124+8n  one row per present member:
            "%-10s " name (colour 8, x=139)
            "%4ld "  HP change, right-aligned in 4 cells (x=205..)
            "* "/"- " W, P, B at x=235/247/259
            "Passed"/"Failed"/"-" at x=271
  y=180   "(C)ontinue"                             colour 0

The emulator's text drawer writes a few pixels in index 12 (brown) that DOSBox
does not (the same artefact is present in combat.png / combat_decision.png), so
the frame is compared by *ink mask* for the text block; the index values of
those pixels are not asserted.
"""
import os
import subprocess
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, "game")
ORACLE = os.path.join(ROOT, "ref", "oracle", "round_result.png")
SAVE = os.path.join(GAME, "GAMES", "GAME2")
SHOT = "/tmp/port_round_result_test.png"
TEXT = (slice(92, 190), slice(139, 317))
ROSTER = (slice(136, 196), slice(6, 136))

EMU_ATTR = [0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17]
STOCK_ATTR = [0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07,
              0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F]


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


def render(shot: str, extra=()) -> np.ndarray:
    # Row specs must follow --round-result immediately (one per present member).
    cmd = [os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
           "--round-result"] + list(extra) + ["--save", SAVE, "--shot", shot]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    return to_index(shot, STOCK_ATTR)


def ink(img, region):
    return img[region] != 7


def fail(msg: str) -> int:
    print("port_round_result_test: FAIL (%s)" % msg)
    return 1


def main() -> int:
    if not os.path.exists(ORACLE):
        print("port_round_result_test: SKIP (no oracle capture)")
        return 0
    if not os.path.exists(SAVE):
        print("port_round_result_test: SKIP (no save fixture)")
        return 0

    port = render(SHOT)
    oracle = to_index(ORACLE, EMU_ATTR)
    if port.shape != oracle.shape:
        return fail("size %s != %s" % (port.shape, oracle.shape))

    # Text block: every inked pixel must match the engine's frame.  (The
    # emulator's index-12 text artefact is ignored; see the module docstring.)
    pi, oi = ink(port, TEXT), ink(oracle, TEXT)
    if not (pi == oi).all():
        return fail("text ink differs (%d px)" % int((pi != oi).sum()))

    # The header/rule/footer rows are solid black text; assert those exactly in
    # index space too, so a colour regression is caught.
    for name, region in (("header", (slice(108, 115), slice(139, 317))),
                         ("rule", (slice(119, 120), slice(139, 317))),
                         ("footer", (slice(180, 187), slice(139, 317)))):
        if not (port[region] == oracle[region]).all():
            return fail("%s row differs" % name)

    # Title carries the ':' glyph (CHR1 band 1 cell 37).  It was missing before
    # this screen existed; pin its ink so the glyph mapping cannot regress.
    if not (ink(port, (slice(92, 99), slice(283, 289))) ==
            ink(oracle, (slice(92, 99), slice(283, 289)))).all():
        return fail("title ':' glyph differs")

    # The W column is CHR1 band 1 cell 30 ('*'): Handy's weapon was damaged in
    # the oracle round, so its cell must be inked and match.
    wcol = (slice(140, 147), slice(235, 240))
    if not (ink(port, wcol) == ink(oracle, wcol)).all():
        return fail("W '*' glyph differs")

    # The table is drawn over the same party/status chrome as the encounter
    # screen; pin that the roster region is unchanged (the viewport creature is
    # the encounter's, so it is not asserted here).
    enc_shot = "/tmp/port_round_result_test_encounter.png"
    subprocess.run([os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
                    "--encounter", "--save", SAVE, "--shot", enc_shot],
                   check=True, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    enc = to_index(enc_shot, STOCK_ATTR)
    if not (port[ROSTER] == enc[ROSTER]).all():
        return fail("roster differs from the encounter screen")

    # Values are inputs to the renderer: flipping a row's flags/outcome must
    # move the corresponding columns and nothing else.
    alt = render("/tmp/port_round_result_test_alt.png",
                 ["0:0:0:0:1", "0:0:0:0:1", "0:1:1:1:2", "0:0:0:0:1",
                  "-7:0:0:0:1", "0:0:0:0:1"])
    row3 = (slice(140, 147), slice(235, 312))
    if (alt[row3] == port[row3]).all():
        return fail("row values do not reach the screen")
    # B and P are now '*'; the attack cell is the out-of-action "-".
    for x in (247, 259):
        if not ink(alt, (slice(140, 147), slice(x, x + 5))).any():
            return fail("P/B column did not render")
    # Everything outside the row is untouched.
    other = np.ones(alt.shape, bool)
    other[row3] = False
    if not (alt[other] == port[other]).all():
        return fail("changing one row altered another region")

    print("port_round_result_test: ok (text ink exact, header/rule/footer "
          "exact, rows driven by input)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
