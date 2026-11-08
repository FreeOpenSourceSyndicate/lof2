#!/usr/bin/env python3
"""Offline explorer for Legend of Faerghail dungeon files (DNGS/DNGnn).

Observations so far (workstream A):

* Every DNG file starts with an escape byte whose value is the RLE marker for
  that file (0xFF for most, 0xFE for DNG01, ...).  The RLE stream that follows is
  the *same* scheme as the EGA images: literal bytes copy through; the escape
  byte introduces a (count, value) pair emitting `value` repeated `count` times.
* Decoding from offset 1 yields exactly 3892 bytes for every file, i.e.
  4 header bytes + 3888 = 3 * 1296 bytes of body (1296 = 36*36).

Header (4 bytes, unverified semantics):
    b0 : ?            b1 : ?
    b2,b3 : a 16-bit little-endian value

Usage:
    python3 tools/dngs.py info  game/DNGS/DNG00
    python3 tools/dngs.py grid  game/DNGS/DNG00 [W] [layer]
    python3 tools/dngs.py layers game/DNGS/DNG00
"""
import collections
import glob
import os
import struct
import sys

HEADER = 4
BODY = 3892 - HEADER          # 3888
LAYER = BODY // 3             # 1296 = 36*36
LAYER_W = 36


def rle_decode(d, esc, start=1):
    out = bytearray()
    i = start
    n = len(d)
    while i < n:
        b = d[i]
        i += 1
        if b == esc:
            if i + 1 >= n:
                break
            cnt = d[i]
            val = d[i + 1]
            i += 2
            out += bytes([val]) * cnt
        else:
            out.append(b)
    return bytes(out)


def decode(path):
    """Return (header_bytes, body) for a DNG file."""
    d = open(path, 'rb').read()
    out = rle_decode(d, d[0], 1)
    return out[:HEADER], out[HEADER:]


def planes(body):
    return [body[i * LAYER:(i + 1) * LAYER] for i in range(3)]


def cell_at(body, layer, x, y, w=LAYER_W):
    return body[layer * LAYER + y * w + x]


def render(body, layer, w=LAYER_W, hi=None):
    blk = body[layer * LAYER:(layer + 1) * LAYER]
    rows = len(blk) // w
    lines = []
    for y in range(rows):
        lines.append(''.join('%x' % v if v else '.' for v in blk[y * w:(y + 1) * w]))
    return lines


def cmd_info(path):
    hdr, body = decode(path)
    v = struct.unpack('<H', hdr[2:4])[0]
    print('%s  size=%d hdr=%s  word=%d (0x%X)' %
          (os.path.basename(path), os.path.getsize(path), hdr.hex(' '), v, v))
    for li, blk in enumerate(planes(body)):
        c = collections.Counter(blk)
        print('  layer %d: top %s  zeros=%d' % (li, c.most_common(6), blk.count(0)))


def cmd_layers(path, w=LAYER_W):
    hdr, body = decode(path)
    for li in range(3):
        print('--- %s layer %d ---' % (os.path.basename(path), li))
        for ln in render(body, li, w):
            print(ln)


def main(argv):
    if len(argv) < 2:
        sys.exit(__doc__)
    cmd = argv[0]
    if cmd == 'info':
        for p in argv[1:]:
            cmd_info(p)
    elif cmd == 'layers':
        cmd_layers(argv[1], int(argv[2]) if len(argv) > 2 else LAYER_W)
    elif cmd == 'all':
        for p in sorted(glob.glob('game/DNGS/DNG*')):
            cmd_info(p)
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main(sys.argv[1:])
