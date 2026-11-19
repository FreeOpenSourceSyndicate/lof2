#!/usr/bin/env python3
"""port_encounter_test — regression test for the native LOF drop-in's encounter
screen.

Composes the wandering-monster encounter with the port (`--encounter`) and
compares it against the emulator's encounter frame (ref/oracle/combat.png, the
first wilderness encounter: four Mercenaries).  This pins the layout recovered
from that frame and from the engine's own data:

  * the encounter is the same HUD background (EGA/UP/MSCR) plus the party/status
    chrome the play screen draws (its panel is byte-identical to the play
    oracle's), with the encountered monster's sprite blitted into the 120x119
    viewport at (10,11);
  * the sprite is the monster record's creature art: SYST/NSC00 record 12
    (Mercenary) has +0x48 = 24, i.e. EGA/CHAR0/CH24.  It is a full 120x119 cell
    blitted plainly at the viewport origin, reproducing the oracle viewport at
    1.0000 (verified for two sprites, CH24 and CH26; see GUESSWORK G-008);
  * the text is drawn in CHR1's full face at a 6-px pitch, x=139: the intro
    ("You unknowingly meet") and "What will you do ?" in colour 8, each group
    line ("4 Mercenaries (Rnk 2)" / "6 Mercenaries (Rnk 1)") with its count and
    name in colour 8 and its " (Rnk R)" tail black, and the four action choices
    ("(A)ttack" ...) black.

The combat oracle was captured from the *emulator*, whose palette is the one the
game programs (index 6 = dark yellow), while the drop-in follows DOSBox (index 6
= brown).  So the two frames are compared in *index* space: each RGB frame is
mapped back to its 0..15 colour indices through its own palette, and the indices
are compared.  The residue is then only the dynamic party/status values a save
does not carry, exactly as on the play screen.

Run via `make golden-port` (or directly).  Needs PIL + numpy and the oracle PNG.
"""
import os
import subprocess
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, "game")
ORACLE = os.path.join(ROOT, "ref", "oracle", "combat.png")
# Second encounter (Elven officer, CHAR0/CH26), captured from the emulator with
# LOF_TICK=0x7777 and pinned so the sprite placement is guarded for a *second*
# creature, not just the Mercenary.  See GUESSWORK G-008.
ORACLE_CH26 = os.path.join(ROOT, "ref", "oracle", "combat_ch26.png")
SAVE = os.path.join(GAME, "GAMES", "GAME2")
SHOT = "/tmp/port_encounter_test.png"
VIEWPORT = (slice(11, 130), slice(10, 130))

# The emulator's combat capture programs its own attribute set (index 6 dark
# yellow); the drop-in follows DOSBox's stock set (index 6 brown, see AGENTS.md).
# So map each frame's RGB back to indices through its own palette.
EMU_ATTR = [0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17]
STOCK_ATTR = [0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07,
              0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F]

# The viewport is pixel-exact bar a few engine pixels; the text region is exact.
# Fail below this so a real regression in the sprite placement or the font
# mapping is caught (the whole-screen residue is the dynamic party/status values).
MIN_MATCH = 0.95
MIN_VIEWPORT = 0.999


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


def _sprite_indices(path):
    """The 120x119 colour-index bitmap of an EGA creature cell."""
    import sys as _s
    _s.path.insert(0, os.path.join(ROOT, "tools"))
    import ega
    return np.array([list(r) for r in ega.indices(path)], dtype=int)


def check_second_encounter():
    """Render the CH26 encounter and verify the sprite sits at the origin."""
    if not os.path.exists(ORACLE_CH26):
        return 0  # optional guard; skip if the capture is absent
    shot2 = "/tmp/port_encounter_ch26.png"
    cmd = [os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
           "--encounter", "16", "4", "1", "--shot", shot2]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    frame = to_index(shot2, STOCK_ATTR)
    window = frame[VIEWPORT]
    sprite = _sprite_indices(os.path.join(GAME, "EGA", "CHAR0", "CH26"))
    if sprite.shape != window.shape:
        print("port_encounter_test: FAIL (CH26 sprite %s != window %s)"
              % (sprite.shape, window.shape))
        return 1
    # (10,11) must reproduce the cell exactly; anything else means the blit
    # moved or the sprite was truncated.
    frac = float((sprite == window).mean())
    if frac < 0.9999:
        print("port_encounter_test: FAIL (CH26 at (10,11) = %.4f, want 1.0000)" % frac)
        return 1
    return 0


def main() -> int:
    if not os.path.exists(ORACLE):
        print("port_encounter_test: SKIP (no oracle capture)")
        return 0
    cmd = [os.path.join(ROOT, "lof"), "--dir", GAME, "--headless",
           "--encounter", "--shot", SHOT]
    if os.path.exists(SAVE):
        cmd += ["--save", SAVE]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    port = to_index(SHOT, STOCK_ATTR)
    oracle = to_index(ORACLE, EMU_ATTR)
    if port.shape != oracle.shape:
        print("port_encounter_test: FAIL (size %s != %s)" % (port.shape, oracle.shape))
        return 1

    match = float((port == oracle).mean())
    vmatch = float((port[VIEWPORT] == oracle[VIEWPORT]).mean())
    if match < MIN_MATCH:
        print("port_encounter_test: FAIL (match=%.1f%% < %.0f%%)"
              % (100 * match, 100 * MIN_MATCH))
        return 1
    if vmatch < MIN_VIEWPORT:
        print("port_encounter_test: FAIL (viewport=%.1f%% < %.0f%%)"
              % (100 * vmatch, 100 * MIN_VIEWPORT))
        return 1

    # The intro line and every action row must match pixel-for-pixel.  The
    # emulator paints a few glyph-descender pixels as index 12 (an emulator-only
    # quirk -- DOSBox's text uses only indices 0/7/15), so compare *ink* masks
    # (non-background), which are palette/artifact independent.
    for y in (92, 132, 156, 164, 172, 180):
        pi = port[y:y + 8, 139:317] != 7
        oi = oracle[y:y + 8, 139:317] != 7
        if not (pi == oi).all():
            print("port_encounter_test: FAIL (text row y=%d ink mismatch)" % y)
            return 1

    # Guard the sprite placement on a second creature.  The encounter viewport
    # is a single full-size cell blitted at the origin, so rendering the CH26
    # encounter and checking that its viewport equals CHAR0/CH26 *at (10,11)*
    # (the unique best offset) catches any regression that only shows up for a
    # sprite other than the pinned Mercenary.  See GUESSWORK G-008.
    rc = check_second_encounter()
    if rc:
        return rc

    print("port_encounter_test: ok (match=%.1f%%, viewport=%.1f%%, text exact, "
          "CH26 placement exact)" % (100 * match, 100 * vmatch))
    return 0


if __name__ == "__main__":
    sys.exit(main())
