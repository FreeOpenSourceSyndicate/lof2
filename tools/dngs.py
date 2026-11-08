#!/usr/bin/env python3
"""Offline explorer for Legend of Faerghail dungeon files (DNGS/DNGnn).

Observations so far (workstream A):

* Every DNG file starts with an escape byte whose value is the RLE marker for
  that file (0xFF for most, 0xFE for DNG01, ...).  The RLE stream that follows is
  the *same* scheme as the EGA images: literal bytes copy through; the escape
  byte introduces a (count, value) pair emitting `value` repeated `count` times.
* Decoding from offset 1 yields exactly 3892 bytes for every file, i.e.
  4 header bytes + 3888 = 1296 tiles + 1296 x 2-byte records (1296 = 36*36).

Decoded layout (3892 bytes):

    0    .. 3    : header (4 bytes; h2|h3 is a 16-bit word, meaning open)
    4    .. 1299 : tile map, 36x36 = 1296 bytes, row-major
    1300 .. 3891 : 1296 two-byte records (feature code in the *high* byte)

The tail is 1296 words, NOT three 1296-byte planes: read as words the feature
bytes are spatially coherent (adjacent non-zero cells), read as planes they are
near-random scatter.  `info` prints the coherence numbers.

Usage:
    python3 tools/dngs.py info  game/DNGS/DNG00 [DNG01 ...]
    python3 tools/dngs.py grid  game/DNGS/DNG00 [tiles|objects] [W]
    python3 tools/dngs.py png   game/DNGS/DNG00 out.png [cell]
    python3 tools/dngs.py all
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


TILE_SYMBOL = {0: '#', 1: '=', 2: '~', 3: '%', 6: '.', 7: '+', 8: 'o', 12: '*'}


def tiles(body):
    """The 1296-byte tile layer (36x36, row-major)."""
    return body[:LAYER]


def objects(body):
    """The 2592-byte tail as 1296 little-endian words: (lo, hi) per cell."""
    tail = body[LAYER:]
    return [(tail[2 * i], tail[2 * i + 1]) for i in range(LAYER)]


def coherence(blk, w=LAYER_W):
    """Adjacent non-zero pairs / non-zero count: high means spatial structure."""
    nz = adj = 0
    for i, v in enumerate(blk):
        if not v:
            continue
        nz += 1
        x, y = i % w, i // w
        if x + 1 < w and blk[i + 1]:
            adj += 1
        if y + 1 < w and i + w < len(blk) and blk[i + w]:
            adj += 1
    return nz, adj


def render_grid(blk, w=LAYER_W, symbol=True):
    rows = len(blk) // w
    out = []
    for y in range(rows):
        if symbol:
            out.append(''.join(TILE_SYMBOL.get(v, '?') if v else ' '
                               for v in blk[y * w:(y + 1) * w]))
        else:
            out.append(''.join('%x' % v if v else '.' for v in blk[y * w:(y + 1) * w]))
    return out


def cmd_info(path):
    hdr, body = decode(path)
    v = struct.unpack('<H', hdr[2:4])[0]
    print('%s  size=%d hdr=%s  word=%d (0x%X)' %
          (os.path.basename(path), os.path.getsize(path), hdr.hex(' '), v, v))
    tl = tiles(body)
    tc = collections.Counter(tl)
    print('  tiles:   top %s' % tc.most_common(6))
    lo = bytes(o[0] for o in objects(body))
    hi = bytes(o[1] for o in objects(body))
    for name, blk in (('obj.lo', lo), ('obj.hi', hi)):
        nz, adj = coherence(blk)
        print('  %s:   nz=%4d adj=%4d (adj/nz=%.2f)' %
              (name, nz, adj, adj / nz if nz else 0.0))


def cmd_grid(path, layer='tiles', w=LAYER_W):
    hdr, body = decode(path)
    if layer == 'tiles':
        blk = tiles(body)
    elif layer == 'objects':
        blk = bytes(o[1] for o in objects(body))
    else:
        sys.exit("layer must be 'tiles' or 'objects'")
    print('--- %s %s (%dx%d) ---' % (os.path.basename(path), layer, w, len(blk) // w))
    for ln in render_grid(blk, w, symbol=(layer == 'tiles')):
        print(ln)


# A tiny, dependency-free PNG writer (grayscale) so the tool works in CI too.
def _png(path, pix, w, h):
    import zlib

    def chunk(tag, data):
        c = tag + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c))

    raw = b''.join(b'\x00' + bytes(pix[y * w:(y + 1) * w]) for y in range(h))
    out = (b'\x89PNG\r\n\x1a\n'
           + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 0, 0, 0, 0))
           + chunk(b'IDAT', zlib.compress(raw, 9))
           + chunk(b'IEND', b''))
    open(path, 'wb').write(out)


TILE_GREY = {0: 32, 6: 200, 3: 96, 12: 64, 1: 140, 2: 120, 7: 170, 8: 150}


def cmd_png(path, out, cell=8):
    hdr, body = decode(path)
    tl = tiles(body)
    obj = objects(body)
    w = h = LAYER_W * cell
    pix = bytearray(w * h)
    for cy in range(LAYER_W):
        for cx in range(LAYER_W):
            g = TILE_GREY.get(tl[cy * LAYER_W + cx], 255)
            if obj[cy * LAYER_W + cx][1]:
                g = 230
            for dy in range(cell):
                row = (cy * cell + dy) * w + cx * cell
                for dx in range(cell):
                    pix[row + dx] = g
    _png(out, pix, w, h)
    print('wrote %s (%dx%d)' % (out, w, h))


def main(argv):
    if len(argv) < 2:
        sys.exit(__doc__)
    cmd = argv[0]
    if cmd == 'info':
        for p in argv[1:]:
            cmd_info(p)
    elif cmd == 'grid':
        cmd_grid(argv[1], argv[2] if len(argv) > 2 else 'tiles',
                 int(argv[3]) if len(argv) > 3 else LAYER_W)
    elif cmd == 'png':
        cmd_png(argv[1], argv[2] if len(argv) > 2 else 'dungeon.png',
                int(argv[3]) if len(argv) > 3 else 8)
    elif cmd == 'all':
        for p in sorted(glob.glob('game/DNGS/DNG*')):
            cmd_info(p)
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main(sys.argv[1:])
