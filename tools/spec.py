#!/usr/bin/env python3
"""Offline explorer for Legend of Faerghail character/item/scenario data.

Workstream B helper.  Prints the structure of SYST/SPECn, ROST/ROST, ROST/SHOP
and GAMES/GAMEn.  It is the tool that produced the layout recorded in
docs/notes-b.md.

The item table is the one exception to "data files only": the engine keeps it
inside LOF.EXE (no data file carries it), so `--emit-items` reads the EXE *once*
as a development aid and regenerates port/char_items.inc, which the drop-in
compiles in instead (it must run without the EXE).

Usage:
    python3 tools/spec.py --dir game              # summary of all files
    python3 tools/spec.py --dir game --spec 0     # dump SPEC0 records
    python3 tools/spec.py --dir game --rost       # dump ROST
    python3 tools/spec.py --dir game --shop       # dump SHOP
    python3 tools/spec.py --dir game --save GAME1 # dump a saved game
    python3 tools/spec.py --dir game --emit-items # regenerate port/char_items.inc
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
        tail = struct.unpack_from("<I", r, 410)[0]
        print(f"  [{i:2d}] present={r[0]:#04x} name={name!r} tail={tail:#x}")


def dump_save(path):
    d = open(path, "rb").read()
    hdr = d[:4]
    n = (len(d) - 4) // 410
    print(f"{path}: size={len(d)} header={hdr.hex()} "
          f"({hdr!r}) records={n} trailer={len(d) - 4 - n * 410}")
    for i in range(n):
        r = d[4 + i * 410:4 + (i + 1) * 410]
        name = cstr(r, 1, 14)
        if r[0] or name:
            print(f"  [{i:2d}] present={r[0]:#04x} name={name!r}")


# The item table lives only inside LOF.EXE (image offset 0x42cec, data segment
# 0x2cec); it is not in any data file.  This is the single place that reads the
# EXE, purely to regenerate the transcribed table the drop-in compiles in.
ITEM_EXE_OFF = 0x42CEC
ITEM_COUNT = 185
ITEM_REC = 32


def emit_items(exe_path, out_path):
    d = open(exe_path, "rb").read()
    names, fields, tail = [], [], []
    for i in range(ITEM_COUNT):
        r = d[ITEM_EXE_OFF + i * ITEM_REC:ITEM_EXE_OFF + (i + 1) * ITEM_REC]
        names.append(r[:16].split(b"\x00")[0].decode("latin1"))
        fields.append(struct.unpack_from("<7h", r, 16))
        tail.append((r[30], r[31]))
    with open(out_path, "w") as f:
        f.write("// Legend of Faerghail item table — transcribed from the DOS "
                "release.\n")
        f.write("// It lives only inside LOF.EXE (image offset 0x42cec / data "
                "segment 0x2cec);\n")
        f.write("// no data file carries it, and the drop-in must run without "
                "the original EXE,\n")
        f.write("// so it keeps this copy.  Regenerate with "
                "`tools/spec.py --emit-items`.\n")
        f.write("// 185 records of 32 bytes: 16-byte NUL-padded name, seven "
                "little-endian\n// int16 fields, then a byte item class and a "
                "byte use-effect index.\n\n")
        f.write("static const char kItemNames[kItemCount][16] = {\n")
        for nm in names:
            f.write('    "%s",\n' % nm.replace("\\", "\\\\").replace('"', '\\"'))
        f.write("};\n\n// The seven int16 fields, then the class and use-effect "
                "bytes.\n")
        f.write("static const int16_t kItemFields[kItemCount][7] = {\n")
        for fs in fields:
            f.write("    {%s},\n" % ",".join(str(v) for v in fs))
        f.write("};\n\nstatic const uint8_t kItemClassEffect[kItemCount][2] = {\n")
        for t in tail:
            f.write("    {%d,%d},\n" % t)
        f.write("};\n")
    print(f"wrote {out_path} ({ITEM_COUNT} items)")


def dump_shop(path):
    d = open(path, "rb").read()
    print(f"{path}: size={len(d)}")
    # Byte 0 header, then one signed byte per item (185, parallel to the item
    # table order), then a 4-byte trailer word.
    adj = [b - 256 if b > 127 else b for b in d[1:1 + ITEM_COUNT]]
    print(f"  header={d[0]:#04x} adjustments({len(adj)})={adj}")
    if len(d) >= 4:
        print(f"  trailer word = {struct.unpack_from('<H', d, len(d) - 4)[0]:#06x}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", default="game")
    ap.add_argument("--spec", type=int, default=None,
                    help="dump one SYST/SPECn (index n)")
    ap.add_argument("--rost", action="store_true")
    ap.add_argument("--shop", action="store_true")
    ap.add_argument("--save", default=None,
                    help="dump a GAMES/GAMEn saved game (e.g. GAME1)")
    ap.add_argument("--emit-items", action="store_true",
                    help="regenerate port/char_items.inc from LOF.EXE")
    ap.add_argument("--nonempty", action="store_true",
                    help="only show SPEC records that carry text")
    args = ap.parse_args()

    if args.emit_items:
        emit_items(os.path.join(args.dir, "LOF.EXE"), "port/char_items.inc")
    elif args.spec is not None:
        dump_spec(os.path.join(args.dir, "SYST", f"SPEC{args.spec}"),
                  args.spec, args.nonempty)
    elif args.rost:
        dump_rost(os.path.join(args.dir, "ROST", "ROST"))
    elif args.shop:
        dump_shop(os.path.join(args.dir, "ROST", "SHOP"))
    elif args.save:
        dump_save(os.path.join(args.dir, "GAMES", args.save))
    else:
        for n in range(8):
            p = os.path.join(args.dir, "SYST", f"SPEC{n}")
            if os.path.exists(p):
                dump_spec(p, n, only_nonempty=True)
        dump_rost(os.path.join(args.dir, "ROST", "ROST"))
        dump_shop(os.path.join(args.dir, "ROST", "SHOP"))


if __name__ == "__main__":
    main()
