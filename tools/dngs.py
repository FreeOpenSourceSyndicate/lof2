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
    python3 tools/dngs.py grid  game/DNGS/DNG00 [tiles|objects|feature] [W]
    python3 tools/dngs.py png   game/DNGS/DNG00 out.png [cell]
    python3 tools/dngs.py doors game/DNGS/DNG00    # door/feature placement
    python3 tools/dngs.py tiles game/EGA          # layered dungeon tilesets
    python3 tools/dngs.py atlas game/EGA D0 out.png  # sheet of a tileset
    python3 tools/dngs.py oracle ref/oracle/dungeon_slot1.png game/EGA 5 1
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
    elif layer == 'feature':
        # same data as objects but printed as hex digits, so a feature code >15
        # is still visible (objects mode maps every code to one symbol).
        blk = bytes(o[1] for o in objects(body))
    else:
        sys.exit("layer must be 'tiles', 'objects' or 'feature'")
    print('--- %s %s (%dx%d) ---' % (os.path.basename(path), layer, w, len(blk) // w))
    for ln in render_grid(blk, w, symbol=(layer == 'tiles')):
        print(ln)


# Solid (wall) tile codes.  PROVISIONAL: derived from connectivity across the
# 29 levels (the {3,12} set keeps 24/29 levels fully connected, which a
# hand-built maze should be) and from the object-feature placement below.  Not
# yet confirmed against an oracle frame; see docs/notes-a.md.
SOLID_TILES = {3, 12}


def _is_solid(tl, x, y, w=LAYER_W, h=LAYER_W):
    if x < 0 or y < 0 or x >= w or y >= h:
        return True
    return tl[y * w + x] in SOLID_TILES


def cmd_doors(path):
    """Classify the object layer: how many features sit on a wall cell that is
    open straight through (a classic doorway), vs on a wall stub or on floor."""
    hdr, body = decode(path)
    tl = tiles(body)
    ob = objects(body)
    n_feat = door = stub = onfloor = 0
    codes = collections.Counter()
    for i in range(LAYER_W * LAYER_W):
        code = ob[i][1]
        if not code:
            continue
        n_feat += 1
        codes[code] += 1
        x, y = i % LAYER_W, i // LAYER_W
        if not _is_solid(tl, x, y):
            onfloor += 1
            continue
        horiz = (not _is_solid(tl, x - 1, y)) and (not _is_solid(tl, x + 1, y))
        vert = (not _is_solid(tl, x, y - 1)) and (not _is_solid(tl, x, y + 1))
        if horiz or vert:
            door += 1
        else:
            stub += 1
    print('%s: %d feature cells -> door-like %d, wall-stub %d, on-floor %d'
          % (os.path.basename(path), n_feat, door, stub, onfloor))
    print('  distinct feature codes: %d  top: %s'
          % (len(codes), codes.most_common(8)))


# First-person viewport preview.  Mirrors render_dungeon_viewport() in
# port/dungeon.cpp: fill the window with black, ray-cast the tile grid, and
# paint a grey (7) wall band whose height is the projected wall height.  Used
# offline to eyeball a level against the DOSBox oracle frames.
def _cast(tl, px, py, dx, dy, maxd, w=LAYER_W, h=LAYER_W):
    mx, my = int(px), int(py)
    ddx = 1e30 if dx == 0 else 1.0 / abs(dx)
    ddy = 1e30 if dy == 0 else 1.0 / abs(dy)
    sx = -1 if dx < 0 else 1
    sy = -1 if dy < 0 else 1
    sidex = (px - mx) if dx < 0 else (mx + 1 - px)
    sidey = (py - my) if dy < 0 else (my + 1 - py)
    sdx, sdy = sidex * ddx, sidey * ddy
    dist = 0.0
    for _ in range(4 * w):
        if sdx < sdy:
            dist, mx, sdx = sdx, mx + sx, sdx + ddx
        else:
            dist, my, sdy = sdy, my + sy, sdy + ddy
        if dist > maxd:
            return maxd
        if _is_solid(tl, mx, my, w, h):
            return dist
    return maxd


def cmd_viewport(path, x=0, y=0, facing=0, vw=120, vh=119):
    """ASCII first-person view from (x,y) facing N/E/S/W (0..3)."""
    _hdr, body = decode(path)
    tl = tiles(body)
    dirs = {0: (0, -1), 1: (1, 0), 2: (0, 1), 3: (-1, 0)}
    dx, dy = dirs[facing % 4]
    plane = 0.57735
    planx, plany = -dy * plane, dx * plane
    px, py = x + 0.5, y + 0.5
    maxd = LAYER_W + LAYER_W
    grid = [['.' for _ in range(vw)] for _ in range(vh)]
    for cx in range(vw):
        cam = 2.0 * cx / vw - 1.0
        rx, ry = dx + planx * cam, dy + plany * cam
        dist = _cast(tl, px, py, rx, ry, maxd)
        wh = min(vh, max(1, int(vh / dist)))
        top = (vh - wh) // 2
        for yy in range(top, top + wh):
            grid[yy][cx] = '#'
    print('%s @(%d,%d) facing %d  tile=%d' %
          (os.path.basename(path), x, y, facing, tl[y * LAYER_W + x]))
    for yy in range(0, vh, 2):
        print(''.join(grid[yy]))


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
    """Render the level as a *walkable* top-down map: floor light, wall dark,
    and object/feature cells (mostly doorways, see cmd_doors) marked bright.
    This mirrors what port/dungeon.cpp::render_dungeon_map draws."""
    hdr, body = decode(path)
    tl = tiles(body)
    obj = objects(body)
    w = h = LAYER_W * cell
    pix = bytearray(w * h)
    for cy in range(LAYER_W):
        for cx in range(LAYER_W):
            v = tl[cy * LAYER_W + cx]
            g = 40 if v in SOLID_TILES else TILE_GREY.get(v, 180)
            if obj[cy * LAYER_W + cx][1]:
                g = 245                    # door/feature marker
            for dy in range(cell):
                row = (cy * cell + dy) * w + cx * cell
                for dx in range(cell):
                    pix[row + dx] = g
    _png(out, pix, w, h)
    print('wrote %s (%dx%d)' % (out, w, h))


def _read_png(path):
    """Minimal PNG reader for DOSBox screenshots (8-bit RGB/RGBA, non-interlaced).

    Returns (w, h, rgb) where rgb[y][x] is an (r,g,b) tuple.  Avoids a PIL
    dependency so the oracle check runs in a bare checkout.
    """
    import zlib
    d = open(path, 'rb').read()
    assert d[:8] == b'\x89PNG\r\n\x1a\n', 'not a PNG: %s' % path
    i = 8
    w = h = bitd = ctype = None
    idat = bytearray()
    plte = b''
    while i < len(d):
        ln = struct.unpack('>I', d[i:i + 4])[0]
        tag = d[i + 4:i + 8]
        body = d[i + 8:i + 8 + ln]
        i += 12 + ln
        if tag == b'IHDR':
            w, h, bitd, ctype = struct.unpack('>IIBB', body[:10])
        elif tag == b'PLTE':
            plte = body
        elif tag == b'IDAT':
            idat += body
        elif tag == b'IEND':
            break
    assert bitd == 8 and ctype in (2, 3, 6), 'unsupported PNG (%d/%d)' % (bitd, ctype)
    nch = {2: 3, 3: 1, 6: 4}[ctype]
    stride = w * nch
    raw = zlib.decompress(bytes(idat))
    out = []
    prev = bytearray(stride)
    pos = 0
    for _ in range(h):
        f = raw[pos]; pos += 1
        line = bytearray(raw[pos:pos + stride]); pos += stride
        for x in range(stride):
            a = line[x - nch] if x >= nch else 0
            b = prev[x]
            c = prev[x - nch] if x >= nch else 0
            if f == 1:
                line[x] = (line[x] + a) & 0xFF
            elif f == 2:
                line[x] = (line[x] + b) & 0xFF
            elif f == 3:
                line[x] = (line[x] + ((a + b) >> 1)) & 0xFF
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[x] = (line[x] + pr) & 0xFF
        if ctype == 3:
            out.append([tuple(plte[line[x] * 3:line[x] * 3 + 3]) for x in range(w)])
        else:
            out.append([tuple(line[x * nch:x * nch + 3]) for x in range(w)])
        prev = line
    return w, h, out


def cmd_oracle(oracle_png, ega_dir, dungeon, index=1):
    """Check the dungeon play-screen composite against a DOSBox oracle frame.

    The engine draws the static frame EGA/UP/MSCR, then blits the view sprite
    EGA/D<dungeon>/DP<index> at (10,11).  This reports the match fraction of the
    frame region, the window, and the whole 320x200 screen, plus the per-sprite
    match so a wrong view index is obvious.  Ground truth:
    ref/oracle/dungeon_slot1.png (dungeon 5, index 1).
    """
    import ega
    w, h, rgb = _read_png(oracle_png)
    pal = ega.EGA_PALETTE

    def to_idx(rgbimg):
        rev = {}
        for k, c in enumerate(pal):
            rev.setdefault(c, k)
        return [[rev.get(rgbimg[y][x], -1) for x in range(len(rgbimg[0]))]
                for y in range(len(rgbimg))]

    orc = to_idx(rgb)
    if (w, h) != (320, 200):
        sys.exit('oracle is %dx%d, expected 320x200' % (w, h))

    def load(rel):
        p = os.path.join(ega_dir, rel)
        iw, ih, _ = ega.decode(p)
        return [list(r) for r in ega.indices(p)]

    frame = load('UP/MSCR')
    view = load('D%d/DP%03d' % (dungeon, index))

    vx, vy = 10, 11
    vh, vw = len(view), len(view[0])

    frame_ok = frame_tot = win_ok = win_tot = tot_ok = 0
    for y in range(h):
        for x in range(w):
            o = orc[y][x]
            inwin = vx <= x < vx + vw and vy <= y < vy + vh
            if inwin:
                ref = view[y - vy][x - vx]
                win_tot += 1
                win_ok += (o == ref)
            else:
                ref = frame[y][x]
                frame_tot += 1
                frame_ok += (o == ref)
            tot_ok += (o == ref)

    print('oracle %s vs MSCR + D%d/DP%03d @(%d,%d)' %
          (os.path.basename(oracle_png), dungeon, index, vx, vy))
    print('  frame region : %d/%d = %.3f' % (frame_ok, frame_tot, frame_ok / frame_tot))
    print('  view  window : %d/%d = %.3f' % (win_ok, win_tot, win_ok / win_tot))
    print('  whole screen : %d/%d = %.3f' % (tot_ok, w * h, tot_ok / (w * h)))
    # Rank every full-size sprite in the tileset so a wrong index stands out.
    full = []
    for f in sorted(glob.glob(os.path.join(ega_dir, 'D%d' % dungeon, 'DP*'))):
        t = load(os.path.join('D%d' % dungeon, os.path.basename(f)))
        if (len(t), len(t[0])) != (vh, vw):
            continue
        ok = sum(t[y][x] == orc[vy + y][vx + x]
                 for y in range(vh) for x in range(vw))
        full.append((ok / (vh * vw), os.path.basename(f)))
    full.sort(reverse=True)
    print('  best full-size sprites: %s' %
          ', '.join('%s %.3f' % (n, s) for s, n in full[:5]))


def cmd_tiles(dn_dir):
    """Inventory the layered dungeon tilesets EGA/Dn/DPnnn (see notes-a.md).

    Each Dn directory is one tileset; the tiles are pre-scaled first-person
    sprites (a wall one step ahead is 120x119, two steps 64x59, ...), so the
    decoded sizes fall into a few discrete depth bands rather than one size.
    """
    import ega
    dirs = sorted(d for d in os.listdir(dn_dir)
                  if os.path.isdir(os.path.join(dn_dir, d)))
    for d in dirs:
        files = sorted(glob.glob(os.path.join(dn_dir, d, 'DP*')))
        if not files:
            continue
        sizes = []
        for f in files:
            w, h, _ = ega.decode(f)
            sizes.append((int(os.path.basename(f)[2:]), w * 8, h))
        bands = collections.Counter((w, h) for _, w, h in sizes)
        print('%s: %d tiles, bands %s' %
              (d, len(sizes), sorted(bands.items(), key=lambda t: -t[0][0])))
        print('   codes: %s' % ' '.join('%03d' % c for c, _, _ in sizes))


def cmd_atlas(dn_dir, tile_set, out, cols=8, bg=0):
    """Write a contact sheet of one dungeon tileset EGA/Dn/DPnnn.

    Each code (from the DPnnn filename) gets one cell, labelled with its code
    so a decoded tile value can be mapped to a sprite by eye once an oracle
    frame is available.  Cells are the sprite's true decoded size (the tiles
    are pre-scaled first-person sprites, so sizes vary), padded into a grid
    whose cell is the largest sprite in the set.
    """
    import ega
    files = sorted(glob.glob(os.path.join(dn_dir, tile_set, 'DP*')))
    if not files:
        sys.exit('no DP* tiles in %s/%s' % (dn_dir, tile_set))
    imgs = []           # (code, width, height, [[idx]])
    for f in files:
        code = int(os.path.basename(f)[2:])
        w, h, _ = ega.decode(f)
        rows = [list(r) for r in ega.indices(f)]
        imgs.append((code, w * 8, h, rows))
    cw = max(w for _, w, _, _ in imgs)
    ch = max(h for _, _, h, _ in imgs)
    rows_n = (len(imgs) + cols - 1) // cols
    W, H = cols * cw, rows_n * ch
    # _png writes 8-bit grayscale, so map EGA indices through the palette.
    lum = [int(0.299 * r + 0.587 * g + 0.114 * b) for r, g, b in ega.EGA_PALETTE]
    pix = bytearray([lum[bg]]) * (W * H)
    for i, (_code, w, h, rows) in enumerate(imgs):
        cx, cy = (i % cols) * cw, (i // cols) * ch
        for y in range(min(h, ch)):
            base = (cy + y) * W + cx
            for x in range(min(w, cw)):
                v = rows[y][x] if y < len(rows) and x < len(rows[y]) else bg
                pix[base + x] = lum[v]
    _png(out, pix, W, H)
    print('wrote %s (%dx%d, %d tiles, cell %dx%d)' %
          (out, W, H, len(imgs), cw, ch))
    print('codes: %s' % ' '.join('%03d' % c for c, _, _, _ in imgs))


def main(argv):
    if len(argv) < 2:
        sys.exit(__doc__)
    cmd = argv[0]
    if cmd == 'tiles':
        cmd_tiles(argv[1] if len(argv) > 1 else 'game/EGA')
    elif cmd == 'info':
        for p in argv[1:]:
            cmd_info(p)
    elif cmd == 'grid':
        cmd_grid(argv[1], argv[2] if len(argv) > 2 else 'tiles',
                 int(argv[3]) if len(argv) > 3 else LAYER_W)
    elif cmd == 'doors':
        for p in argv[1:]:
            cmd_doors(p)
    elif cmd == 'viewport':
        # dngs.py viewport <file> [x y facing]
        x = int(argv[2]) if len(argv) > 2 else 0
        y = int(argv[3]) if len(argv) > 3 else 0
        fa = int(argv[4]) if len(argv) > 4 else 0
        cmd_viewport(argv[1], x, y, fa)
    elif cmd == 'png':
        cmd_png(argv[1], argv[2] if len(argv) > 2 else 'dungeon.png',
                int(argv[3]) if len(argv) > 3 else 8)
    elif cmd == 'atlas':
        cmd_atlas(argv[1] if len(argv) > 1 else 'game/EGA',
                  argv[2] if len(argv) > 2 else 'D0',
                  argv[3] if len(argv) > 3 else 'atlas.png',
                  int(argv[4]) if len(argv) > 4 else 8)
    elif cmd == 'oracle':
        # dngs.py oracle <oracle.png> <ega_dir> <dungeon> [index]
        cmd_oracle(argv[1], argv[2] if len(argv) > 2 else 'game/EGA',
                   int(argv[3]) if len(argv) > 3 else 5,
                   int(argv[4]) if len(argv) > 4 else 1)
    elif cmd == 'all':
        for p in sorted(glob.glob('game/DNGS/DNG*')):
            cmd_info(p)
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main(sys.argv[1:])
