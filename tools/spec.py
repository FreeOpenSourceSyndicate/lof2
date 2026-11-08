#!/usr/bin/env python3
"""Offline explorer for Legend of Faerghail character/item/scenario data.

Workstream B helper.  Reads the game *data* files only (never the EXEs) and
prints the structure of SYST/SPECn, ROST/ROST and ROST/SHOP.  It is the tool
that produced the layout recorded in docs/notes-b.md.

Usage:
    python3 tools/spec.py --dir game              # summary of all files
    python3 tools/spec.py --dir game --spec 0     # dump SPEC0 records
    python3 tools/spec.py --dir game --rost       # dump ROST
    python3 tools/spec.py --dir game --shop       # dump SHOP
"""
import argparse
import os
import struct

SPEC_REC = 238   # 0xee, confirmed in analysis/lof.asm
ROST_REC = 414

# Dispatch-type labels, derived by correlating the value at 0xe2 with each
# record's text across all 665 records (kept in sync with port/char.cpp).
TYPE_NAME = {
    0: "text", 1: "recruit", 2: "spring", 3: "spring2", 4: "battle",
    5: "passage", 6: "empty6", 7: "door", 8: "trade", 9: "sign",
    10: "spring3", 11: "teleport", 12: "button", 13: "inscription",
    14: "challenge", 15: "empty15", 16: "lava", 17: "puzzle", 18: "trap",
    19: "button19", 20: "button20", 21: "button21", 22: "button22",
    24: "door24", 25: "elemental",
}


def cstr(b, off, limit):
    end = off
    while end < off + limit and end < len(b) and b[end]:
        end += 1
    return b[off:end].decode("latin1")


def dump_spec(path, index=None, only_nonempty=False):
    d = open(path, "rb").read()
    count = d[0]
    print(f"{path}: id={index} size={len(d)} count={count} "
          f"records={count} stride={SPEC_REC} "
          f"ok={(len(d) == 1 + count * SPEC_REC)}")
    for i in range(count):
        r = d[1 + i * SPEC_REC:1 + (i + 1) * SPEC_REC]
        ta = cstr(r, 0x0e, 0x4a - 0x0e)
        tb = cstr(r, 0x50, 0xc6 - 0x50)
        if only_nonempty and not (ta or tb):
            continue
        typ = struct.unpack_from("<H", r, 0xE2)[0]
        idx = r[0xC9]
        a4c = struct.unpack_from("<H", r, 0x4C)[0]
        a4e = struct.unpack_from("<h", r, 0x4E)[0]
        cnt = struct.unpack_from("<H", r, 0xE4)[0]
        print(f"  [{i:3d}] type={typ:<3d} ({TYPE_NAME.get(typ,'?'):<11}) "
              f"idx={idx:#04x} a4c={a4c} a4e={a4e} cnt={cnt}")
        if ta:
            print(f"        a: {ta}")
        if tb:
            print(f"        b: {tb}")


def dump_rost(path):
    d = open(path, "rb").read()
    count = d[0]
    print(f"{path}: size={len(d)} count={count} stride={ROST_REC} "
          f"ok={(len(d) == 1 + count * ROST_REC)}")
    for i in range(count):
        r = d[1 + i * ROST_REC:1 + (i + 1) * ROST_REC]
        name = cstr(r, 1, 14)
        print(f"  [{i:2d}] present={r[0]:#04x} name={name!r} "
              f"tail={r[410:414].hex()}")


def dump_shop(path):
    d = open(path, "rb").read()
    print(f"{path}: size={len(d)}")
    adj = [b - 256 if b > 127 else b for b in d[1:46]]
    print(f"  header={d[0]:#04x} adjustments({len(adj)})={adj}")
    if len(d) >= 4:
        print(f"  trailing word = {struct.unpack_from('<H', d, len(d) - 4)[0]:#06x}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", default="game")
    ap.add_argument("--spec", type=int, default=None,
                    help="dump one SYST/SPECn (index n)")
    ap.add_argument("--rost", action="store_true")
    ap.add_argument("--shop", action="store_true")
    ap.add_argument("--nonempty", action="store_true",
                    help="only show SPEC records that carry text")
    args = ap.parse_args()

    if args.spec is not None:
        dump_spec(os.path.join(args.dir, "SYST", f"SPEC{args.spec}"),
                  args.spec, args.nonempty)
    elif args.rost:
        dump_rost(os.path.join(args.dir, "ROST", "ROST"))
    elif args.shop:
        dump_shop(os.path.join(args.dir, "ROST", "SHOP"))
    else:
        for n in range(8):
            p = os.path.join(args.dir, "SYST", f"SPEC{n}")
            if os.path.exists(p):
                dump_spec(p, n, only_nonempty=True)
        dump_rost(os.path.join(args.dir, "ROST", "ROST"))
        dump_shop(os.path.join(args.dir, "ROST", "SHOP"))


if __name__ == "__main__":
    main()
