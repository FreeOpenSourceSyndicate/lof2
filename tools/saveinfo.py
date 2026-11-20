#!/usr/bin/env python3
"""saveinfo.py — report a Legend of Faerghail save's identity from its bytes.

Prints a one-line summary and a suggested descriptive archive name.  Used by
`tools/ingest-save.sh` to file uploads under content-unique names, and handy on
its own when triaging a save the owner drops in.

The offsets are the ones the engine uses (see port/char.h):
  0x000  header magic 00 'F' 'O' 'L'
  0x9a2  location ones digit          -> map DNGS/DNG<tens><ones>
  0x9a4  location tens digit          -> also the town selector (0=Cyldane,
                                          nonzero=Thyn; the engine reuses it)
  0x9a6  facing (0=N,1=E,2=S,3=W)
  0x9b6  time-of-day index (dword)
  0x9ba  morale index (dword)
  0x004  first of floor((size-4)/410) 410-byte character records; +0x00 is the
         present flag and +0x01 the NUL-terminated name.

Usage:  tools/saveinfo.py FILE [FILE...]
        tools/saveinfo.py --name FILE     # only the suggested archive name
Exit status is 0 for a valid save, 1 for anything else.
"""
import hashlib
import os
import struct
import sys

MAGIC = b"\x00FOL"
REC = 410
FACING = ("N", "E", "S", "W")


def parse(path):
    d = open(path, "rb").read()
    if len(d) < 4 or d[:4] != MAGIC:
        return None
    ones = d[0x9A2] if len(d) > 0x9A2 else 0
    tens = d[0x9A4] if len(d) > 0x9A4 else 0
    facing = d[0x9A6] if len(d) > 0x9A6 else 0xFF
    tod = struct.unpack("<I", d[0x9B6:0x9BA])[0] if len(d) >= 0x9BA else -1
    morale = struct.unpack("<I", d[0x9BA:0x9BE])[0] if len(d) >= 0x9BE else -1
    names = []
    n = (len(d) - 4) // REC
    for i in range(n):
        r = d[4 + i * REC: 4 + (i + 1) * REC]
        if r[0] == 1:  # engine's "slot in use" marker (see party_status())
            nm = r[1:15].split(b"\x00")[0].decode("latin1")
            if nm:
                names.append(nm)
    return {
        "md5": hashlib.md5(d).hexdigest(),
        "size": len(d),
        "ones": ones,
        "tens": tens,
        "map": "DNG%01d%01d" % (tens, ones),
        # 0x9a4 is the location tens digit; the engine also reads it as the
        # town-name selector (0 -> Cyldane, nonzero -> Thyn) when in town.
        "town_sel": tens,
        "facing": FACING[facing] if facing < 4 else "?",
        "tod": tod,
        "morale": morale,
        "party": names,
    }


def slug(info):
    """A short, filesystem-safe description: map + first party member."""
    first = info["party"][0] if info["party"] else "empty"
    s = "%s-%s" % (info["map"].lower(), first)
    return "".join(c if c.isalnum() or c in "-_" else "-" for c in s).strip("-")


def main(argv):
    only_name = False
    if argv and argv[0] == "--name":
        only_name = True
        argv = argv[1:]
    if not argv:
        sys.stderr.write(__doc__)
        return 2
    rc = 0
    for path in argv:
        info = parse(path)
        if info is None:
            print("%s: not a LoF save (bad magic)" % path)
            rc = 1
            continue
        if only_name:
            print(slug(info))
            continue
        print("%s" % path)
        print("  md5     %s" % info["md5"])
        print("  size    %d" % info["size"])
        print("  location %s (ones=%d tens=%d)  facing=%s" % (
            info["map"], info["ones"], info["tens"], info["facing"]))
        print("  time-of-day %d  morale %d" % (info["tod"], info["morale"]))
        print("  party   %s" % (", ".join(info["party"]) or "(none)"))
        print("  name    %s" % slug(info))
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
