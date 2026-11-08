#!/usr/bin/env python3
"""Decode Legend of Faerghail EGA image files.

Format (little-endian), reverse-engineered from LOF.EXE / VOR.EXE and verified
against the emulator's own rendering:

    off 0 : escape byte (RLE marker; the stream starts right after the header)
    off 1 : 0x1234 magic (word)
    off 3 : width  in bytes per row (word)   -> pixel width = width*8
    off 5 : height in rows          (word)
    off 7 : RLE stream

RLE stream: literal bytes are copied as-is; the escape byte introduces a
(count, value) pair that emits `value` repeated `count` times.  The decoded
buffer is 4 EGA bitplanes laid out consecutively, each `width*height` bytes:

    plane0 (blue)    offset 0
    plane1 (green)   offset w*h
    plane2 (red)     offset 2*w*h
    plane3 (intensity) offset 3*w*h

The colour index of a pixel is bit p from plane p, with the most-significant bit
of each byte being the leftmost pixel.
"""
import struct


def decode(path):
    """Return (w_bytes, h, planes) where planes is 4 concatenated bitplanes."""
    d = open(path, 'rb').read()
    if len(d) < 7 or d[1:3] != b'\x34\x12':
        raise ValueError('%s: bad 0x1234 magic' % path)
    esc = d[0]
    w = struct.unpack('<H', d[3:5])[0]
    h = struct.unpack('<H', d[5:7])[0]
    out = bytearray()
    i = 7
    n = len(d)
    while i < n:
        b = d[i]; i += 1
        if b == esc:
            if i + 1 >= n:
                break
            cnt = d[i]; val = d[i + 1]; i += 2
            out += bytes([val]) * cnt
        else:
            out.append(b)
    return w, h, bytes(out)


# Standard EGA 16-colour palette (RGB), used when the game does not override it.
EGA_PALETTE = [
    (0, 0, 0), (0, 0, 170), (0, 170, 0), (0, 170, 170),
    (170, 0, 0), (170, 0, 170), (170, 85, 0), (170, 170, 170),
    (85, 85, 85), (85, 85, 255), (85, 255, 85), (85, 255, 255),
    (255, 85, 85), (255, 85, 255), (255, 255, 85), (255, 255, 255),
]


def indices(path):
    """Yield rows of colour indices (0..15) for the image."""
    w, h, planes = decode(path)
    pl = w * h
    for y in range(h):
        row = []
        base = y * w
        for xb in range(w):
            o = base + xb
            b0 = planes[o] if o < len(planes) else 0
            b1 = planes[pl + o] if pl + o < len(planes) else 0
            b2 = planes[2 * pl + o] if 2 * pl + o < len(planes) else 0
            b3 = planes[3 * pl + o] if 3 * pl + o < len(planes) else 0
            for bit in range(8):
                s = 7 - bit
                row.append(((b0 >> s) & 1) | (((b1 >> s) & 1) << 1) |
                           (((b2 >> s) & 1) << 2) | (((b3 >> s) & 1) << 3))
        yield row


def to_image(path, palette=None):
    from PIL import Image
    pal = palette or EGA_PALETTE
    w, h, _planes = decode(path)
    img = Image.new('RGB', (w * 8, h))
    px = img.load()
    for y, row in enumerate(indices(path)):
        for x, idx in enumerate(row):
            px[x, y] = pal[idx]
    return img


if __name__ == '__main__':
    import sys
    for p in sys.argv[1:]:
        w, h, planes = decode(p)
        print('%-24s esc=%02X  %3d B/row x %3d rows  planes=%d  expect=%d'
              % (p, open(p, 'rb').read()[0], w, h, len(planes), 4 * w * h))
