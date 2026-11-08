#!/usr/bin/env python3
"""MZ (DOS .EXE) loader, relocation and disassembly helper."""
import sys, struct
from capstone import Cs, CS_ARCH_X86, CS_MODE_16

def load(path):
    d = open(path, 'rb').read()
    if d[:2] != b'MZ':
        raise ValueError('not MZ')
    lastblk, pages, nreloc, hdrsz, minalloc, maxalloc, ss, sp, chk, ip, cs, reltbl, ovl = \
        struct.unpack('<HHHHHHHHHHHHH', d[2:28])
    hdr = hdrsz * 16
    img = bytearray(d[hdr:])
    relocs = []
    for i in range(nreloc):
        o = reltbl + i * 4
        off, seg = struct.unpack('<HH', d[o:o+4])
        relocs.append((seg, off))
    return dict(data=d, hdr=hdr, image=img, relocs=relocs, ss=ss, sp=sp, cs=cs, ip=ip,
                minalloc=minalloc, maxalloc=maxalloc)

def flat(loadseg, img, relocs):
    """Return flat image with relocations applied relative to loadseg."""
    out = bytearray(img)
    for seg, off in relocs:
        p = seg * 16 + off
        if p + 2 <= len(out):
            v = out[p] | (out[p+1] << 8)
            v = (v + loadseg) & 0xFFFF
            out[p] = v & 0xFF
            out[p+1] = v >> 8
    return out

def disasm(img, start=0, end=None):
    md = Cs(CS_ARCH_X86, CS_MODE_16)
    md.detail = False
    end = end if end is not None else len(img)
    return list(md.disasm(bytes(img[start:end]), start))

def main():
    cmd = sys.argv[1]
    path = sys.argv[2]
    m = load(path)
    if cmd == 'info':
        print(path)
        print('  image size : %d' % len(m['image']))
        print('  header     : 0x%X' % m['hdr'])
        print('  relocations: %d' % len(m['relocs']))
        print('  entry      : %04X:%04X' % (m['cs'], m['ip']))
        print('  stack      : %04X:%04X' % (m['ss'], m['sp']))
        print('  minalloc   : %d paragraphs' % m['minalloc'])
    elif cmd == 'tally':
        from collections import Counter
        mn = Counter()
        for ins in disasm(m['image']):
            mn[ins.mnemonic] += 1
        for k, v in mn.most_common():
            print('%8d %s' % (v, k))

if __name__ == '__main__':
    main()
