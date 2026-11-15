#!/usr/bin/env python3
"""Disassemble a byte range of the LOF.EXE image.

`analysis/lof.asm` is the annotated listing, but its recursive descent only
labels addresses it actually reaches; helpers and data-adjacent routines are
shown as raw bytes.  This tool decodes any image range directly from the
on-disk executable (the same image `tools/decompile.py` uses), so you can read a
routine the listing skipped.

Usage:
    python3 tools/lofdis.py <start> <length> [--exe PATH]

`<start>` and `<length>` are hex (0x prefix optional).  `<start>` is an **image
offset** (the address shown in the listing's first column, i.e. file offset
minus the MZ header size).  The MZ header is located automatically.

Two mappings are printed for convenience, because the same address has three
forms in this project:

  * image offset                 — first column of analysis/lof.asm
  * `cs:ip` under the emulator   — the image is loaded at segment 0x1010, so
                                   `cs = 0x1010 + (off >> 4)`, `ip = off & 0xF`
  * the flat `seg:off` used by `lcall` operands in the listing, whose segment is
    relative to the image start (segment = off >> 4).

Requires `capstone` (`pip install capstone`).
"""
import argparse
import os
import struct
import sys

try:
    from capstone import Cs, CS_ARCH_X86, CS_MODE_16
except ImportError:
    raise SystemExit("lofdis: needs capstone (pip install capstone)")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_EXE = os.path.join(ROOT, "game", "LOF.EXE")

# The emulator loads the program image at this segment (see src/machine.cpp).
LOAD_SEG = 0x1010


def load_image(path):
    d = open(path, "rb").read()
    if d[:2] != b"MZ":
        raise SystemExit("%s: not an MZ executable" % path)
    hdrsz = struct.unpack("<H", d[8:10])[0]
    return d[hdrsz * 16:]


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("start", help="image offset to start at (hex)")
    ap.add_argument("length", help="number of bytes to decode (hex)")
    ap.add_argument("--exe", default=DEFAULT_EXE, help="path to LOF.EXE")
    args = ap.parse_args()

    start = int(args.start, 16)
    length = int(args.length, 16)
    img = load_image(args.exe)
    if start < 0 or start + length > len(img):
        raise SystemExit("range 0x%X+0x%X outside image (len 0x%X)"
                         % (start, length, len(img)))

    seg = start >> 4
    print("; image 0x%05X  cs:ip %04X:%04X  flat seg:off %04X:%04X"
          % (start, LOAD_SEG + seg, start & 0xF, seg, start & 0xF))
    md = Cs(CS_ARCH_X86, CS_MODE_16)
    for ins in md.disasm(img[start:start + length], start):
        print("%05X: %-20s %s %s" % (ins.address, ins.bytes.hex(),
                                     ins.mnemonic, ins.op_str))


if __name__ == "__main__":
    sys.exit(main())
