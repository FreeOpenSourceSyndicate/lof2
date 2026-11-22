#!/usr/bin/env python3
"""The dungeon answer-input path (P-2).

A SPEC record with a keyword (+0x00) reads a reply before anything fires
(0x209C8..0x20A95): only a matching answer reaches the per-type effect switch.
The outcome line (+0x0e) is printed on both branches, gated by +0x4b.

Drives the real `lof --walk` path over GAME2 moved to the recruit cell
(SPEC0[1], "A large warrior asks if he may join you (Yes/No)."):

  no --answer   -> "(answer wanted)", no outcome line, no effect
  --answer N    -> the outcome line ("He goes, disappointed."), no effect
  --answer Y    -> the outcome line AND the effect (the recruit is reported)

The N/Y control matters: it proves the effect is gated on the answer rather
than on entering the cell.
"""
import os
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOF = os.path.join(ROOT, "lof")
GAME = os.path.join(ROOT, "game")
SAVE = os.path.join(GAME, "GAMES", "GAME2")
START_CELL = 1 * 36 + 31          # the recruit cell is (31,2), facing south
FORWARD = "0x48"
OUTCOME = "He goes, disappointed."


def checksum(data):
    acc = 1
    for i, byte in enumerate(data):
        value = byte if byte else 1
        acc = (acc + value) & 0xFFFFFFFF if i % 2 == 0 else acc ^ value
    return acc & 0xFFFFFFFF


def make_save(path):
    data = bytearray(open(SAVE, "rb").read())
    struct.pack_into("<H", data, 0x9A0, START_CELL)
    data[0x9A6] = 2
    struct.pack_into("<I", data, 0x2E3A, checksum(data[:0x2E3A]))
    with open(path, "wb") as f:
        f.write(data)


def run(save, answer=None):
    cmd = [LOF, "--dir", GAME, "--walk", "--save", save, "--walk-keys", FORWARD]
    if answer is not None:
        cmd += ["--answer", answer]
    p = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT)
    if p.returncode != 0:
        sys.exit("answer_path_test: lof failed: %s" % p.stderr.strip())
    return p.stdout


def main():
    if not os.path.exists(LOF):
        print("answer_path_test: skip (no ./lof)")
        return
    save = "/tmp/answer_path_test.sav"
    make_save(save)

    quiet = run(save)
    assert "(answer wanted)" in quiet, quiet
    assert OUTCOME not in quiet, "the outcome line printed before any answer"
    assert "would join" not in quiet, "the effect fired without an answer"

    decline = run(save, "N")
    assert OUTCOME in decline, decline
    assert "would join" not in decline, "a wrong answer fired the effect"

    accept = run(save, "Y")
    assert OUTCOME in accept, accept
    assert "would join" in accept, "a correct answer did not fire the effect"

    os.remove(save)
    print("answer_path_test: ok (no answer -> prompt only; N -> line, no effect; "
          "Y -> line + effect)")


if __name__ == "__main__":
    main()
