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
            hp = struct.unpack_from("<h", r, 0x22)[0]
            hpmax = struct.unpack_from("<h", r, 0x20)[0]
            # 32-bit experience: low word +0x6c, high word +0x6e.
            exp = struct.unpack_from("<I", r, 0x6c)[0]
            print(f"  [{i:2d}] present={r[0]:#04x} name={name!r} "
                  f"rnk={r[0x17]} con={r[0x44]} str={r[0x45]} dex={r[0x46]} "
                  f"int={r[0x47]} wis={r[0x48]} hp={hp}/{hpmax} exp={exp} "
                  f"body={r[0x24]} mods={r[0x5b]}/{r[0x5c]}/{r[0x5e]}/{r[0x5f]} "
                  f"flag={r[0x6a]}")


# The item table lives only inside LOF.EXE (image offset 0x42cec, data segment
# 0x2cec); it is not in any data file.  This is the single place that reads the
# EXE, purely to regenerate the transcribed table the drop-in compiles in.
ITEM_EXE_OFF = 0x42CEC
ITEM_COUNT = 185
ITEM_REC = 32


def c_string(name):
    """Escape a name for a C string literal: octal for non-printable bytes so
    the emitted source stays ASCII regardless of the compiler's encoding."""
    out = []
    for b in name.encode("latin1"):
        if 32 <= b < 127 and b not in (ord('"'), ord("\\")):
            out.append(chr(b))
        else:
            out.append("\\%03o" % b)
    return "".join(out)


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
            f.write('    "%s",\n' % c_string(nm))
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


# The spell table also lives only inside LOF.EXE (image offset 0x44430, data
# segment 0x4430): 141 records of 36 bytes — a 22-byte NUL-padded name then 14
# data bytes.  The records are grouped by caster class (magician, druid, ...);
# the last 14 carry German names (unused/debug).  Like the item table, the
# drop-in must run without the EXE, so it is transcribed here.
SPELL_EXE_OFF = 0x44430
SPELL_COUNT = 141
SPELL_REC = 36
SPELL_NAME = 22


def emit_spells(exe_path, out_path):
    d = open(exe_path, "rb").read()
    names, data = [], []
    for i in range(SPELL_COUNT):
        r = d[SPELL_EXE_OFF + i * SPELL_REC:SPELL_EXE_OFF + (i + 1) * SPELL_REC]
        names.append(r[:SPELL_NAME].split(b"\x00")[0].decode("latin1").rstrip())
        data.append(r[SPELL_NAME:SPELL_REC])
    with open(out_path, "w") as f:
        f.write("// Legend of Faerghail spell table — transcribed from the DOS "
                "release.\n")
        f.write("// It lives only inside LOF.EXE (image offset 0x44430 / data "
                "segment 0x4430);\n")
        f.write("// no data file carries it, and the drop-in must run without "
                "the original EXE,\n")
        f.write("// so it keeps this copy.  Regenerate with "
                "`tools/spec.py --emit-spells`.\n")
        f.write("// 141 records of 36 bytes: a 22-byte NUL-padded name then 14 "
                "data bytes.\n\n")
        f.write("static const char kSpellNames[kSpellCount][22] = {\n")
        for nm in names:
            f.write('    "%s",\n' % c_string(nm))
        f.write("};\n\n// The 14 data bytes per spell.\n")
        f.write("static const uint8_t kSpellData[kSpellCount][14] = {\n")
        for ds in data:
            f.write("    {%s},\n" % ",".join(str(b) for b in ds))
        f.write("};\n")
    print(f"wrote {out_path} ({SPELL_COUNT} spells)")


# Character-generation name tables (also only inside LOF.EXE).  The
# sex/race/profession/alignment tables are fixed 12-byte records (11 chars +
# NUL), zero-padded between groups; skills and languages are packed
# variable-length NUL-terminated strings.
SEX_OFF, SEX_N = 0x41E28, 2
RACE_OFF, RACE_N = 0x41E4A, 6
PROF_OFF, PROF_N = 0x41E98, 12
ALIGN_OFF, ALIGN_N = 0x41F28, 2
SKILL_OFF, SKILL_N = 0x41D52, 9
LANG_OFF, LANG_N = 0x41DBE, 8
STATUS_OFF, STATUS_N = 0x42190, 8  # short forms, 6-byte records
STATUS_LONG_OFF = 0x421C0        # long forms, packed
# Character display tables used by the party/status screens.
GENDERPROF_OFF, GENDERPROF_N = 0x41F87, 12  # packed "female X" / "Priestess"
RACEADJ_OFF, RACEADJ_N = 0x4202E, 6         # packed "Human-", "Elven-", ...
ALIGNLOWER_OFF, ALIGNLOWER_N = 0x42013, 2   # packed "lawful ", "chaotic "
PROFABBR_OFF, PROFABBR_N, PROFABBR_REC = 0x420DA, 13, 3
PROF_PADDED_OFF, PROF_PADDED_N, PROF_PADDED_REC = 0x42101, 13, 11
ACTION_OFF, ACTION_N = 0x42204, 5           # 12-byte records
MORALE_OFF, MORALE_N, MORALE_REC = 0x423F8, 5, 13
COMBAT_ABBR_OFF, COMBAT_ABBR_N, COMBAT_ABBR_REC = 0x421F0, 4, 5
SCENARIO_OFF, SCENARIO_N, SCENARIO_REC = 0x42240, 8, 22  # maps scenario idx
FACING_OFF, FACING_N, FACING_REC = 0x422F0, 4, 22   # compass heading line
TIMEOFDAY_OFF, TIMEOFDAY_N, TIMEOFDAY_REC = 0x42348, 8, 22  # clock line
NAME_REC = 12


def _fixed_names(d, off, n, rec=NAME_REC):
    return [d[off + i * rec:off + i * rec + rec]
            .split(b"\x00")[0].decode("latin1").rstrip() for i in range(n)]


def _packed_names(d, off, n):
    out, i = [], off
    for _ in range(n):
        j = d.find(b"\x00", i)
        out.append(d[i:j].decode("latin1"))
        i = j + 1
    return out


def emit_names(exe_path, out_path):
    d = open(exe_path, "rb").read()
    groups = [
        ("kSexNames", _fixed_names(d, SEX_OFF, SEX_N)),
        ("kRaceNames", _fixed_names(d, RACE_OFF, RACE_N)),
        ("kProfessionNames", _fixed_names(d, PROF_OFF, PROF_N)),
        ("kAlignmentNames", _fixed_names(d, ALIGN_OFF, ALIGN_N)),
        ("kSkillNames", _packed_names(d, SKILL_OFF, SKILL_N)),
        ("kLanguageNames", _packed_names(d, LANG_OFF, LANG_N)),
        ("kStatusShortNames", _fixed_names(d, STATUS_OFF, STATUS_N, 6)),
        ("kStatusLongNames", _packed_names(d, STATUS_LONG_OFF, STATUS_N)),
        ("kGenderProfessionNames", _packed_names(d, GENDERPROF_OFF, GENDERPROF_N)),
        ("kRaceAdjectiveNames", _packed_names(d, RACEADJ_OFF, RACEADJ_N)),
        ("kAlignmentLowerNames", _packed_names(d, ALIGNLOWER_OFF, ALIGNLOWER_N)),
        ("kProfessionAbbrevNames",
         _fixed_names(d, PROFABBR_OFF, PROFABBR_N, PROFABBR_REC)),
        ("kProfessionPaddedNames",
         _fixed_names(d, PROF_PADDED_OFF, PROF_PADDED_N, PROF_PADDED_REC)),
        ("kActionNames", _fixed_names(d, ACTION_OFF, ACTION_N)),
        ("kMoraleNames", _fixed_names(d, MORALE_OFF, MORALE_N, MORALE_REC)),
        ("kCombatAbbrevNames",
         _fixed_names(d, COMBAT_ABBR_OFF, COMBAT_ABBR_N, COMBAT_ABBR_REC)),
        ("kScenarioNames",
         _fixed_names(d, SCENARIO_OFF, SCENARIO_N, SCENARIO_REC)),
        ("kFacingNames",
         _fixed_names(d, FACING_OFF, FACING_N, FACING_REC)),
        ("kTimeOfDayNames",
         _fixed_names(d, TIMEOFDAY_OFF, TIMEOFDAY_N, TIMEOFDAY_REC)),
    ]
    with open(out_path, "w") as f:
        f.write("// Legend of Faerghail character-generation name tables — "
                "transcribed\n// from the DOS release (LOF.EXE image offsets "
                "0x41e28/0x41e4a/0x41e98/0x41f28\n// for sex/race/profession/"
                "alignment, 0x41d52 skills, 0x41dbe languages).\n"
                "// Regenerate with `tools/spec.py --emit-names`.\n\n")
        for sym, names in groups:
            f.write("static const char* const %s[] = {\n" % sym)
            for nm in names:
                f.write('    "%s",\n' % c_string(nm))
            f.write("};\n\n")
    print(f"wrote {out_path} ({sum(len(n) for _, n in groups)} names)")


def dump_names(exe_path):
    d = open(exe_path, "rb").read()
    print(f"sex      : {_fixed_names(d, SEX_OFF, SEX_N)}")
    print(f"race     : {_fixed_names(d, RACE_OFF, RACE_N)}")
    print(f"profession: {_fixed_names(d, PROF_OFF, PROF_N)}")
    print(f"alignment: {_fixed_names(d, ALIGN_OFF, ALIGN_N)}")
    print(f"skills   : {_packed_names(d, SKILL_OFF, SKILL_N)}")
    print(f"languages: {_packed_names(d, LANG_OFF, LANG_N)}")
    print(f"status sh: {_fixed_names(d, STATUS_OFF, STATUS_N, 6)}")
    print(f"status lg: {_packed_names(d, STATUS_LONG_OFF, STATUS_N)}")


def dump_shop(path):
    d = open(path, "rb").read()
    print(f"{path}: size={len(d)}")
    # Byte 0 header, then one signed byte per item (185, parallel to the item
    # table order), then a 4-byte trailer word.
    adj = [b - 256 if b > 127 else b for b in d[1:1 + ITEM_COUNT]]
    print(f"  header={d[0]:#04x} adjustments({len(adj)})={adj}")
    if len(d) >= 4:
        print(f"  trailer word = {struct.unpack_from('<H', d, len(d) - 4)[0]:#06x}")


def dump_spells(exe_path):
    d = open(exe_path, "rb").read()
    print(f"{exe_path}: spell table at {SPELL_EXE_OFF:#x}, {SPELL_COUNT} records")
    for i in range(SPELL_COUNT):
        r = d[SPELL_EXE_OFF + i * SPELL_REC:SPELL_EXE_OFF + (i + 1) * SPELL_REC]
        nm = r[:SPELL_NAME].split(b"\x00")[0].decode("latin1").rstrip()
        print(f"  [{i:3d}] {nm:22s} {r[SPELL_NAME:].hex(' ')}")


def dump_sns(path):
    """Dump an SNSC/NSC file (78-byte records) and the stats-block -> character
    seed mapping decoded from analysis/lof.asm 1DA8E..1DCBB."""
    d = open(path, "rb").read()
    if not d:
        print(f"{path}: empty")
        return
    # SNSC is 14 bare 78-byte records (no count byte: 1092 = 14*78); NSCnn is a
    # count byte followed by that many records (1951 = 1 + 25*78).
    if len(d) % 78 == 0:
        base, count = 0, len(d) // 78
    else:
        base, count = 1, d[0]
    print(f"{path}: count={count} stride=78")
    # record-relative offset -> (char offset, transform)
    fields = [
        (0x4c, "+0x18", "copy", "class flag (1 PC / 2 monster)"),
        (0x2e, "+0x1c", "copy", "base stat"),
        (0x2d, "+0x1e", "100-n", "body stat"),
        (0x2d, "+0x1d", "100-n", "seed (overridden by Dex at 1BA40)"),
        (0x36, "+0x56", "copy", "speed"),
        (0x2a, "+0x7a+n", "set 2", "seed-slot index"),
    ]
    for i in range(count):
        r = d[base + i * 78:base + i * 78 + 78]
        if len(r) < 78:
            break
        sing = r[0:19].split(b"\x00")[0].decode("latin1")
        plur = r[19:38].split(b"\x00")[0].decode("latin1")
        print(f"  [{i:2d}] {sing!r}/{plur!r} stats={r[38:78].hex(' ')}")
        parts = []
        for off, ch, tf, _ in fields:
            v = r[off]
            out = (100 - v) & 0xFF if tf == "100-n" else v
            parts.append(f"rec[{off:#x}]={v}->{ch}={out}")
        # Four (value, level) skill pairs -> char +0x142+2n / +0x143+2n.
        for n in range(4):
            parts.append(f"rec[{0x3b + 2 * n:#x}]={r[0x3b + 2 * n]}"
                         f"/{r[0x3c + 2 * n]}->+{0x142 + 2 * n:#x}"
                         f"/+{0x143 + 2 * n:#x}")
        print(f"        seed: " + "  ".join(parts))


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
    ap.add_argument("--emit-spells", action="store_true",
                    help="regenerate port/char_spells.inc from LOF.EXE")
    ap.add_argument("--spells", action="store_true",
                    help="dump the LOF.EXE spell table")
    ap.add_argument("--emit-names", action="store_true",
                    help="regenerate port/char_names.inc from LOF.EXE")
    ap.add_argument("--names", action="store_true",
                    help="dump the LOF.EXE char-gen name tables")
    ap.add_argument("--sns", default=None,
                    help="dump an SNSC/NSC file and its character-seed mapping "
                         "(e.g. SNSC or NSC00)")
    ap.add_argument("--nonempty", action="store_true",
                    help="only show SPEC records that carry text")
    args = ap.parse_args()

    if args.emit_items:
        emit_items(os.path.join(args.dir, "LOF.EXE"), "port/char_items.inc")
    elif args.emit_spells:
        emit_spells(os.path.join(args.dir, "LOF.EXE"), "port/char_spells.inc")
    elif args.spells:
        dump_spells(os.path.join(args.dir, "LOF.EXE"))
    elif args.emit_names:
        emit_names(os.path.join(args.dir, "LOF.EXE"), "port/char_names.inc")
    elif args.names:
        dump_names(os.path.join(args.dir, "LOF.EXE"))
    elif args.sns:
        dump_sns(os.path.join(args.dir, "SYST", args.sns))
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
