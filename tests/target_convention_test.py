"""Pin the two address conventions for LOF.EXE (see docs/notes-a.md Round 27).

Conflating them is not a theoretical risk: it produced the "the event dispatcher
has no callers" error that stalled P-2 for a whole session.  This test fails if
either helper drifts.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "tools"))
from lofdis import LOAD_SEG, from_linear, load_image, target  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(ROOT, "game", "LOF.EXE")

# lcall operands -> image offsets, with the first bytes expected at the target.
# Only routines that really do start with `push bp; mov bp, sp` (55 8b ec) are
# listed that way; 0x1B77E is `xor ax,ax` and is pinned by its bytes instead.
LCALL_CASES = [
    ((0x1CFE, 0x233E), 0x1F31E, b"\x55\x8b\xec", "SPEC dispatcher"),
    ((0x1CFE, 0x1FE4), 0x1EFC4, b"\x55\x8b\xec", "SPEC key-table builder"),
    ((0x2F0E, 0x1AF4), 0x30BD4, b"\x55\x8b\xec", "string length"),
    ((0x1B39, 0x03EE), 0x1B77E, b"\x33\xc0", "screen clear"),
]


def main():
    img = load_image(EXE)

    for (seg, off), want, prologue, what in LCALL_CASES:
        got = target(seg, off)
        assert got == want, "%s: target(%04X,%04X)=%05X want %05X" % (
            what, seg, off, got, want)
        # The plain form must be exactly seg*16+off -- no hidden bias.
        assert got == seg * 16 + off
        assert img[got:got + len(prologue)] == prologue, (
            "%s: unexpected bytes at %05X (got %s)" % (
                what, got, img[got:got + 3].hex()))

    # A run-time linear address subtracts the load segment, and must agree with
    # the lcall form for the same routine: the dispatcher is flat 0x2F41E.
    assert from_linear(0x2F41E) == 0x1F31E
    assert from_linear(0x2F41E) == target(0x1CFE, 0x233E)
    assert from_linear(0) == -LOAD_SEG * 16

    # The two forms must NOT be interchangeable.  Applying the linear rule to an
    # lcall operand does not crash -- it silently lands *inside* the image, at
    # 0xF21E, in the middle of an instruction with no prologue.  That silence is
    # precisely why the mistake survived: the search for callers found nothing
    # and looked like a real negative result.
    wrong = target(0x1CFE, 0x233E) - LOAD_SEG * 16
    assert wrong != 0x1F31E
    assert 0 <= wrong < len(img)                      # silently in-image...
    assert img[wrong:wrong + 3] != b"\x55\x8b\xec"     # ...but not a function
    # The reverse mistake is just as silent: treating the run-time linear
    # address 0x2F41E as an image offset also lands in-image (at 0x2F41E, bytes
    # 01 75 ca eb) rather than failing loudly.  Neither wrong mapping raises.
    assert 0x2F41E < len(img)
    assert img[0x2F41E:0x2F41E + 3] != b"\x55\x8b\xec"

    print("target_convention_test: ok (4 lcall targets, linear mapping, both traps)")


if __name__ == "__main__":
    main()
