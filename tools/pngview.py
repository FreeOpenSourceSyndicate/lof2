#!/usr/bin/env python3
"""Render a PNG as ASCII art (no external deps)."""
import struct
import sys
import zlib


def load(path):
    d = open(path, 'rb').read()
    p = 8
    w = h = 0
    idat = b''
    while p < len(d):
        ln = struct.unpack('>I', d[p:p + 4])[0]
        typ = d[p + 4:p + 8]
        data = d[p + 8:p + 8 + ln]
        if typ == b'IHDR':
            w, h, bd, ct = struct.unpack('>IIBB', data[:10])
        elif typ == b'IDAT':
            idat += data
        p += 12 + ln
    raw = zlib.decompress(idat)
    return w, h, raw


def main():
    w, h, raw = load(sys.argv[1])
    cols = int(sys.argv[2]) if len(sys.argv) > 2 else 100
    stride = 1 + w * 3
    chars = ' .:-=+*#%@'
    sy = max(1, h // 40)
    sx = max(1, w // cols)
    for y in range(0, h, sy * 2):
        row = []
        for x in range(0, w, sx):
            o = y * stride + 1 + x * 3
            v = (raw[o] + raw[o + 1] + raw[o + 2]) // 3
            row.append(chars[min(9, v * 10 // 256)])
        print(''.join(row))


if __name__ == '__main__':
    main()
