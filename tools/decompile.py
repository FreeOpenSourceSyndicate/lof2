#!/usr/bin/env python3
"""Decompile the Legend of Faerghail executables.

Produces an annotated 16-bit x86 disassembly listing for LOF.EXE and VOR.EXE.
VOR.EXE is LZEXE-packed; its own unpacking stub is emulated (the same one the
native loader runs) so the listing is of the real, relocated code the game
executes, not of the packer.

Usage:
    python3 tools/decompile.py lof [outfile]
    python3 tools/decompile.py vor [outfile]

Output defaults to analysis/<name>.asm.  The listing annotates:
  * entry points, call/jmp targets (as labels)
  * INT nn calls (BIOS/DOS/mouse services)
  * far calls into the overlay/loader segments
  * printable strings referenced via immediate or DS:offset operands
"""
import os
import struct
import sys

from capstone import Cs, CS_ARCH_X86, CS_MODE_16

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))



def load_mz(path):
    d = open(path, 'rb').read()
    if d[:2] != b'MZ':
        raise SystemExit('%s: not an MZ executable' % path)
    (lastblk, pages, nreloc, hdrsz, minalloc, maxalloc,
     ss, sp, chk, ip, cs, reltbl, ovl) = struct.unpack('<HHHHHHHHHHHHH', d[2:28])
    hdr = hdrsz * 16
    image = bytearray(d[hdr:])
    relocs = []
    for i in range(nreloc):
        o = reltbl + i * 4
        off, seg = struct.unpack('<HH', d[o:o + 4])
        relocs.append((seg, off))
    return dict(image=image, hdr=hdr, relocs=relocs, ss=ss, sp=sp, cs=cs, ip=ip)


def unpack_vor_image(path):
    """Return (image, entry_ip) for the VOR payload.

    VOR.EXE is LZEXE-packed.  Rather than re-implement the packer's token
    format here, we let the native loader do it: run the emulator for a single
    frame with a RAM dump, then slice out the unpacked image.  This guarantees
    the listing matches the code the game actually executes.
    """
    import subprocess
    import tempfile

    exe = os.path.join(ROOT, 'lof-native')
    if not os.path.exists(exe):
        raise SystemExit('build lof-native first (make) so VOR can be unpacked')
    fd, dump = tempfile.mkstemp(prefix='vor_ram_', suffix='.bin')
    os.close(fd)
    env = dict(os.environ, LOF_DUMP=dump)
    subprocess.run([exe, '--dir', os.path.join(ROOT, 'game'), '--headless',
                    '--exe', path, '--frames', '1'],
                   env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                   timeout=120, check=False)
    ram = open(dump, 'rb').read()
    os.unlink(dump)
    # Loader places the program at PSP+0x10 paragraphs; VOR's unpacked image is
    # <= 64 KiB.  Trim trailing zero padding.
    base = 0x10100
    image = bytearray(ram[base:base + 0x10000])
    while len(image) > 0 and image[-1] == 0:
        image.pop()
    # LZEXE stores the original entry point (ip, cs) at the top of the payload.
    entry = struct.unpack('<H', bytes(image[:2]))[0] if len(image) >= 2 else 0
    # VOR's entry, per its own header, is cs:ip = 3A4:001E inside the image.
    return image, 0x3A5E


def cstr(image, off, maxlen=48):
    if off < 0 or off >= len(image):
        return None
    s = bytearray()
    while off < len(image) and image[off] not in (0, 0x0A, 0x0D) and len(s) < maxlen:
        b = image[off]
        if b < 0x20 or b > 0x7E:
            return None
        s.append(b)
        off += 1
    return s.decode('ascii') if len(s) >= 4 else None


INT_NAMES = {
    0x10: 'VIDEO', 0x11: 'EQUIP', 0x12: 'MEM', 0x13: 'DISK',
    0x16: 'KBD', 0x17: 'PRINTER', 0x1A: 'TIME', 0x21: 'DOS',
    0x33: 'MOUSE', 0xFA: 'VOR',
}


def decompile(image, entry, name, out):
    md = Cs(CS_ARCH_X86, CS_MODE_16)
    md.detail = True

    # Recursive-descent disassembly: follow control flow from the entry point
    # instead of sweeping linearly, so data regions are not mis-decoded and
    # reachable code (including far-called overlay stubs) is covered.
    n = len(image)
    code = {}          # addr -> CsInsn
    work = [entry & 0xFFFF]

    def target(ins):
        """Offset operand of a near or far transfer (far: 'seg, off')."""
        op = ins.op_str
        if op.startswith('0x'):
            return int(op.split(',')[0], 16) & 0xFFFF
        return None

    while work:
        a = work.pop()
        while 0 <= a < n and a not in code:
            ins = next(md.disasm(bytes(image[a:a + 15]), a), None)
            if ins is None or ins.size == 0:
                break
            code[a] = ins
            a += ins.size
            m = ins.mnemonic
            if m in ('ret', 'retf', 'iret', 'hlt'):
                break
            if m in ('jmp', 'ljmp'):
                t = target(ins)
                if t is not None:
                    work.append(t)
                break
            if m in ('call', 'lcall'):
                t = target(ins)
                if t is not None:
                    work.append(t)
            elif m.startswith('j') or m in ('loop', 'loopne', 'loope', 'jcxz'):
                t = target(ins)
                if t is not None:
                    work.append(t)

    insns = [code[a] for a in sorted(code)]
    targets = set()
    for ins in insns:
        if ins.op_str.startswith('0x'):
            try:
                t = int(ins.op_str, 16) & 0xFFFF
                if t in code:
                    targets.add(t)
            except ValueError:
                pass
    labels = targets | {(entry & 0xFFFF)}

    out.write('; Legend of Faerghail — annotated disassembly of %s\n' % name)
    out.write('; image size = %d bytes, entry = %04X, %d instructions reached\n'
              % (len(image), entry, len(insns)))
    out.write('; generated by tools/decompile.py (recursive descent)\n\n')

    int_counts = {}
    prev_end = None
    for ins in insns:
        if prev_end is not None and ins.address != prev_end:
            out.write('        ...             ; (data / unreached bytes skipped)\n')
        prev_end = ins.address + ins.size
        if ins.address in labels:
            out.write('\n%04X:                          ; ---- label ----\n' % ins.address)
        line = '%04X: %-22s %s' % (ins.address, ins.bytes.hex(), ins.mnemonic)
        if ins.op_str:
            line += ' ' + ins.op_str
        note = ''
        if ins.mnemonic == 'int':
            try:
                k = int(ins.op_str, 16)
                int_counts[k] = int_counts.get(k, 0) + 1
                note = '  ; INT %02Xh %s' % (k, INT_NAMES.get(k, ''))
            except ValueError:
                pass
        elif ins.mnemonic in ('mov', 'cmp', 'lea') and ', 0x' in ins.op_str:
            try:
                imm = int(ins.op_str.split(', 0x')[-1], 16)
                s = cstr(image, imm)
                if s:
                    note = '  ; "%s"' % s
            except (ValueError, IndexError):
                pass
        out.write(line + note + '\n')

    out.write('\n; ---- interrupt usage summary ----\n')
    for k in sorted(int_counts):
        out.write(';   INT %02Xh %-8s x%d\n' % (k, INT_NAMES.get(k, ''), int_counts[k]))


def main():
    if len(sys.argv) < 2 or sys.argv[1] not in ('lof', 'vor'):
        raise SystemExit('usage: decompile.py {lof|vor} [outfile]')
    which = sys.argv[1]
    path = os.path.join(ROOT, 'game', 'LOF.EXE' if which == 'lof' else 'VOR.EXE')
    outpath = sys.argv[2] if len(sys.argv) > 2 else \
        os.path.join(ROOT, 'analysis', which + '.asm')

    if which == 'lof':
        m = load_mz(path)
        image, entry = m['image'], ((m['cs'] << 4) + m['ip'])
        name = 'LOF.EXE (main game)'
    else:
        image, entry = unpack_vor_image(path)
        name = 'VOR.EXE (unpacked payload)'

    os.makedirs(os.path.dirname(outpath), exist_ok=True)
    with open(outpath, 'w') as f:
        decompile(image, entry, name, f)
    print('wrote %s (%d bytes of code)' % (outpath, len(image)))


if __name__ == '__main__':
    main()
