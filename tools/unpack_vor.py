#!/usr/bin/env python3
"""Unpack the LZEXE byte-token variant used by VOR.EXE.

Token semantics recovered from the packer stub:
  0xb0/0xb1, count:u16, value:u8  -> RLE run of `count` copies of `value`
  0xb2/0xb3, count:u16, data      -> literal block copy of `count` bytes
  anything else                   -> single literal byte (the token itself)
"""
import struct
import sys


def decompress(d, start, limit=None):
    out = bytearray()
    i = start
    n = len(d)
    while i < n:
        tok = d[i]
        i += 1
        base = tok & 0xFE
        if base == 0xB0:
            if i + 3 > n:
                return out, i, 'eof'
            cnt = d[i] | (d[i + 1] << 8)
            i += 2
            val = d[i]
            i += 1
            out += bytes([val]) * cnt
        elif base == 0xB2:
            if i + 2 > n:
                return out, i, 'eof'
            cnt = d[i] | (d[i + 1] << 8)
            i += 2
            if i + cnt > n:
                return out, i, 'eof'
            out += d[i:i + cnt]
            i += cnt
        else:
            return out, i, 'bad'
        if tok & 1:                       # last block marker
            return out, i, 'done'
        if limit and len(out) >= limit:
            return out, i, 'limit'
    return out, i, 'eof'


def try_offsets(path):
    d = open(path, 'rb').read()

    def g(o):
        return struct.unpack('<H', d[o:o + 2])[0]

    hdr = g(8) * 16
    e_cs, e_ip = g(22), g(20)
    stub_entry = hdr + e_cs * 16 + e_ip
    inf_off = hdr + e_cs * 16
    inf = struct.unpack('<8H', d[inf_off:inf_off + 16])
    ip, cs, sp, ss, comp_par, incr_par, stub_bytes, chk = inf
    target = cs * 16 + ip
    print('header', hex(hdr), 'stub_entry', hex(stub_entry), 'inf@', hex(inf_off))
    print('inf: ip=%04x cs=%04x sp=%04x ss=%04x comp=%04x incr=%04x stub=%04x chk=%04x'
          % (ip, cs, sp, ss, comp_par, incr_par, stub_bytes, chk))
    print('decompressed entry image offset =', hex(target))
    for start in range(hdr, len(d)):
        out, i, why = decompress(d, start, limit=target + 64)
        if len(out) >= target + 4 and out[target:target + 2] == b'\xb4\x30':
            print('CANDIDATE start=0x%X outlen>=%d entry=%s' %
                  (start, len(out), out[target:target + 8].hex()))
            return start, target, out
    return None, target, None


if __name__ == '__main__':
    start, target, out = try_offsets(sys.argv[1])
    print('start', start, 'outlen', len(out) if out else None)
    if out:
        s = out.find(b'MS Run-Time Library')
        print('MS string at', hex(s) if s >= 0 else None)
