// 8086 instruction interpreter for the Legend of Faerghail native port.
#include "emu.h"

namespace lof {

// ------------------------------------------------------------- registers ---
uint8_t CPU::get8(int i) {
    static const int idx[8] = {0, 1, 2, 3, 0, 1, 2, 3};
    static const int hi[8] = {0, 0, 0, 0, 1, 1, 1, 1};
    uint16_t v = reg16(idx[i]);
    return hi[i] ? (uint8_t)(v >> 8) : (uint8_t)(v & 0xFF);
}
void CPU::set8(int i, uint8_t val) {
    static const int idx[8] = {0, 1, 2, 3, 0, 1, 2, 3};
    static const int hi[8] = {0, 0, 0, 0, 1, 1, 1, 1};
    uint16_t& r = reg16(idx[i]);
    if (hi[i]) r = (uint16_t)((r & 0x00FF) | (val << 8));
    else       r = (uint16_t)((r & 0xFF00) | val);
}

// --------------------------------------------------------------- helpers ---
static inline uint16_t pf_of(uint8_t v) {
    v ^= v >> 4; v ^= v >> 2; v ^= v >> 1;
    return (v & 1) ? 0 : PF;
}

struct Decoder {
    CPU& c;
    Machine& m;
    uint8_t* mem;
    explicit Decoder(CPU& cpu) : c(cpu), m(*cpu.m), mem(cpu.m->mem()) {}

    uint8_t fetch8() { return mem[(c.cs * 16u + c.ip++) & 0xFFFFF]; }
    uint16_t fetch16() { uint16_t a = fetch8(); return (uint16_t)(a | (fetch8() << 8)); }
    int8_t fetchs8() { return (int8_t)fetch8(); }

    uint16_t seg_of(int def) {
        if (c.has_ovr) return c.ovr_seg;
        return def == 0 ? c.es : def == 1 ? c.cs : def == 2 ? c.ss : c.ds;
    }

    uint16_t ea(uint8_t mod, uint8_t rm, int& defseg) {
        uint16_t base = 0; defseg = 3;
        switch (rm) {
            case 0: base = (uint16_t)(c.bx + c.si); break;
            case 1: base = (uint16_t)(c.bx + c.di); break;
            case 2: base = (uint16_t)(c.bp + c.si); defseg = 2; break;
            case 3: base = (uint16_t)(c.bp + c.di); defseg = 2; break;
            case 4: base = c.si; break;
            case 5: base = c.di; break;
            case 6: if (mod == 0) base = fetch16(); else { base = c.bp; defseg = 2; } break;
            default: base = c.bx; break;
        }
        if (mod == 1) base = (uint16_t)(base + fetchs8());
        else if (mod == 2) base = (uint16_t)(base + fetch16());
        return base;
    }

    uint32_t phys(uint16_t seg, uint16_t off) { return ((uint32_t)seg << 4) + off; }

    uint16_t rm_read(uint8_t mod, uint8_t rm, bool is8, int& defseg) {
        if (mod == 3) return is8 ? c.get8(rm) : c.reg16(rm);
        uint16_t off = ea(mod, rm, defseg);
        uint32_t a = phys(seg_of(defseg), off);
        return is8 ? m.rb(a) : m.rw(a);
    }
    void rm_write(uint8_t mod, uint8_t rm, bool is8, uint16_t val, int& defseg) {
        if (mod == 3) { if (is8) c.set8(rm, (uint8_t)val); else c.reg16(rm) = val; return; }
        uint16_t off = ea(mod, rm, defseg);
        uint32_t a = phys(seg_of(defseg), off);
        if (is8) m.wb(a, (uint8_t)val); else m.ww(a, val);
    }

    // Read an r/m operand and remember its resolved address so the matching
    // write does not re-decode (and re-consume) the displacement.  addr is set
    // to 0xFFFFFFFF for register operands.
    uint16_t rm_read2(uint8_t mod, uint8_t rm, bool is8, uint32_t& addr) {
        if (mod == 3) { addr = 0xFFFFFFFFu; return is8 ? c.get8(rm) : c.reg16(rm); }
        int defs; uint16_t off = ea(mod, rm, defs);
        addr = phys(seg_of(defs), off);
        return is8 ? m.rb(addr) : m.rw(addr);
    }
    void rm_write2(uint32_t addr, uint8_t mod, uint8_t rm, bool is8, uint16_t val) {
        if (mod == 3) { if (is8) c.set8(rm, (uint8_t)val); else c.reg16(rm) = val; return; }
        if (is8) m.wb(addr, (uint8_t)val); else m.ww(addr, val);
    }

    void push16(uint16_t v) { c.sp = (uint16_t)(c.sp - 2); m.ww(phys(c.ss, c.sp), v); }
    uint16_t pop16() { uint16_t v = m.rw(phys(c.ss, c.sp)); c.sp = (uint16_t)(c.sp + 2); return v; }
};

// ---------------------------------------------------------------- ALU ------
static uint16_t alu(CPU& c, int op, uint16_t a, uint16_t b, bool is8) {
    unsigned mask = is8 ? 0xFF : 0xFFFF;
    unsigned sign = is8 ? 0x80 : 0x8000;
    unsigned res = 0;
    uint16_t f = (uint16_t)(c.flags & ~(CF | PF | AF | ZF | SF | OF));
    switch (op) {
        case 0: res = (a + b) & mask;
            if (((a & b) | ((a ^ b) & ~res)) & sign) f |= OF;
            if ((a + b) & (sign << 1)) f |= CF;
            if ((a ^ b ^ res) & 0x10) f |= AF;
            break;
        case 1: res = (a | b) & mask; break;
        case 2: { unsigned t = a + b + ((c.flags & CF) ? 1u : 0u); res = t & mask;
            if (((a & b) | ((a ^ b) & ~res)) & sign) f |= OF;
            if (t & (sign << 1)) f |= CF;
            if ((a ^ b ^ res) & 0x10) f |= AF;
            break; }
        case 3: { unsigned t = a - b - ((c.flags & CF) ? 1u : 0u); res = t & mask;
            if (((a ^ b) & (a ^ res)) & sign) f |= OF;
            if (t & (sign << 1)) f |= CF;
            if ((a ^ b ^ res) & 0x10) f |= AF;
            break; }
        case 4: res = (a & b) & mask; break;
        case 5: res = (a - b) & mask;
            if (((a ^ b) & (a ^ res)) & sign) f |= OF;
            if (a < b) f |= CF;
            if ((a ^ b ^ res) & 0x10) f |= AF;
            break;
        case 6: res = (a ^ b) & mask; break;
        default: res = (a - b) & mask;
            if (((a ^ b) & (a ^ res)) & sign) f |= OF;
            if (a < b) f |= CF;
            if ((a ^ b ^ res) & 0x10) f |= AF;
            break;
    }
    if (res & sign) f |= SF;
    if (res == 0) f |= ZF;
    f |= pf_of((uint8_t)res);
    c.flags = f;
    return (uint16_t)res;
}

static void logic_flags(CPU& c, uint16_t res, bool is8) {
    unsigned sign = is8 ? 0x80 : 0x8000;
    uint16_t f = (uint16_t)(c.flags & ~(CF | PF | AF | ZF | SF | OF));
    res &= is8 ? 0xFF : 0xFFFF;
    if (res & sign) f |= SF;
    if (res == 0) f |= ZF;
    f |= pf_of((uint8_t)res);
    c.flags = f;
}

static void inc_flags(CPU& c, uint16_t a, uint16_t res, bool is8) {
    unsigned sign = is8 ? 0x80 : 0x8000;
    uint16_t f = (uint16_t)(c.flags & ~(PF | AF | ZF | SF | OF));
    if (!(a & sign) && (res & sign)) f |= OF;
    if ((a ^ res) & 0x10) f |= AF;
    if (res & sign) f |= SF;
    if (res == 0) f |= ZF;
    f |= pf_of((uint8_t)res);
    c.flags = f;
}
static void dec_flags(CPU& c, uint16_t a, uint16_t res, bool is8) {
    unsigned sign = is8 ? 0x80 : 0x8000;
    uint16_t f = (uint16_t)(c.flags & ~(PF | AF | ZF | SF | OF));
    if ((a & sign) && !(res & sign)) f |= OF;
    if ((a ^ res) & 0x10) f |= AF;
    if (res & sign) f |= SF;
    if (res == 0) f |= ZF;
    f |= pf_of((uint8_t)res);
    c.flags = f;
}

static uint16_t shift_op(CPU& c, int op, uint16_t v, int count, bool is8) {
    unsigned mask = is8 ? 0xFF : 0xFFFF;
    unsigned sign = is8 ? 0x80 : 0x8000;
    unsigned bits = is8 ? 8 : 16;
    uint16_t f = c.flags;
    if (count <= 0) return v;
    unsigned r = v & mask;
    for (int i = 0; i < count; i++) {
        unsigned cf;
        switch (op) {
            case 0: cf = (r >> (bits - 1)) & 1; r = ((r << 1) | cf) & mask; break;
            case 1: cf = r & 1; r = (r >> 1) | (cf << (bits - 1)); break;
            case 2: cf = (r >> (bits - 1)) & 1; r = ((r << 1) | ((f & CF) ? 1u : 0u)) & mask; break;
            case 3: cf = r & 1; r = (r >> 1) | (((f & CF) ? 1u : 0u) << (bits - 1)); break;
            case 4: case 6: cf = (r >> (bits - 1)) & 1; r = (r << 1) & mask; break;
            case 5: cf = r & 1; r = r >> 1; break;
            default: cf = r & 1; r = (r >> 1) | (r & sign); break;
        }
        f = (uint16_t)((f & ~CF) | (cf ? CF : 0));
    }
    if (count == 1) {
        bool of = false;
        switch (op) {
            case 0: case 2: case 4: case 6: of = (((r >> (bits - 1)) & 1) ^ ((f & CF) ? 1 : 0)) != 0; break;
            case 1: case 3: of = (((r >> (bits - 1)) & 1) ^ ((r >> (bits - 2)) & 1)) != 0; break;
            case 5: of = (v & sign) != 0; break;
            default: of = false; break;
        }
        f = (uint16_t)((f & ~OF) | (of ? OF : 0));
    }
    f = (uint16_t)(f & ~(PF | ZF | SF));
    if (r & sign) f |= SF;
    if (r == 0) f |= ZF;
    f |= pf_of((uint8_t)r);
    c.flags = f;
    return (uint16_t)r;
}

static int cond_true(CPU& c, int cc) {
    bool cf = c.flags & CF, zf = c.flags & ZF, sf = c.flags & SF,
         of = c.flags & OF, pf = c.flags & PF;
    switch (cc) {
        case 0x0: return of;
        case 0x1: return !of;
        case 0x2: return cf;
        case 0x3: return !cf;
        case 0x4: return zf;
        case 0x5: return !zf;
        case 0x6: return cf || zf;
        case 0x7: return !cf && !zf;
        case 0x8: return sf;
        case 0x9: return !sf;
        case 0xA: return pf;
        case 0xB: return !pf;
        case 0xC: return sf != of;
        case 0xD: return sf == of;
        case 0xE: return zf || (sf != of);
        default:  return !zf && (sf == of);
    }
}

// ------------------------------------------------------------- step -------
void cpu_step(CPU& c) {
    Machine& m = *c.m;
    Decoder d(c);
    c.has_ovr = false;
    c.ovr_seg = 0;
    c.rep = 0;

    for (;;) {
        uint32_t pc = (uint32_t)c.cs * 16u + c.ip;
        uint8_t op = m.rb(pc);
        if (op == 0x26) { c.has_ovr = true; c.ovr_seg = c.es; c.ip++; continue; }
        if (op == 0x2E) { c.has_ovr = true; c.ovr_seg = c.cs; c.ip++; continue; }
        if (op == 0x36) { c.has_ovr = true; c.ovr_seg = c.ss; c.ip++; continue; }
        if (op == 0x3E) { c.has_ovr = true; c.ovr_seg = c.ds; c.ip++; continue; }
        if (op == 0xF0 || op == 0xF2 || op == 0xF3) {
            if (op != 0xF0) c.rep = op;
            c.ip++; continue;
        }
        break;
    }

    uint8_t op = d.fetch8();
    int defseg;

    switch (op) {
        case 0x00: case 0x08: case 0x10: case 0x18: case 0x20: case 0x28: case 0x30: case 0x38: {
            uint8_t modrm = d.fetch8(); int aluop = (op >> 3) & 7;
            uint32_t addr; uint16_t dst = d.rm_read2(modrm >> 6, modrm & 7, true, addr);
            uint16_t src = c.get8((modrm >> 3) & 7);
            uint16_t r = alu(c, aluop, dst, src, true);
            if (aluop != 7) d.rm_write2(addr, modrm >> 6, modrm & 7, true, r);
            break;
        }
        case 0x01: case 0x09: case 0x11: case 0x19: case 0x21: case 0x29: case 0x31: case 0x39: {
            uint8_t modrm = d.fetch8(); int aluop = (op >> 3) & 7;
            uint32_t addr; uint16_t dst = d.rm_read2(modrm >> 6, modrm & 7, false, addr);
            uint16_t src = c.reg16((modrm >> 3) & 7);
            uint16_t r = alu(c, aluop, dst, src, false);
            if (aluop != 7) d.rm_write2(addr, modrm >> 6, modrm & 7, false, r);
            break;
        }
        case 0x02: case 0x0A: case 0x12: case 0x1A: case 0x22: case 0x2A: case 0x32: case 0x3A: {
            uint8_t modrm = d.fetch8(); int aluop = (op >> 3) & 7;
            uint16_t dst = c.get8((modrm >> 3) & 7);
            uint16_t src = d.rm_read(modrm >> 6, modrm & 7, true, defseg);
            uint16_t r = alu(c, aluop, dst, src, true);
            if (aluop != 7) c.set8((modrm >> 3) & 7, (uint8_t)r);
            break;
        }
        case 0x03: case 0x0B: case 0x13: case 0x1B: case 0x23: case 0x2B: case 0x33: case 0x3B: {
            uint8_t modrm = d.fetch8(); int aluop = (op >> 3) & 7;
            uint16_t dst = c.reg16((modrm >> 3) & 7);
            uint16_t src = d.rm_read(modrm >> 6, modrm & 7, false, defseg);
            uint16_t r = alu(c, aluop, dst, src, false);
            if (aluop != 7) c.reg16((modrm >> 3) & 7) = r;
            break;
        }
        case 0x04: case 0x0C: case 0x14: case 0x1C: case 0x24: case 0x2C: case 0x34: case 0x3C: {
            uint16_t r = alu(c, (op >> 3) & 7, c.ax & 0xFF, d.fetch8(), true);
            if (((op >> 3) & 7) != 7) c.ax = (uint16_t)((c.ax & 0xFF00) | r);
            break; }
        case 0x05: case 0x0D: case 0x15: case 0x1D: case 0x25: case 0x2D: case 0x35: case 0x3D: {
            uint16_t r = alu(c, (op >> 3) & 7, c.ax, d.fetch16(), false);
            if (((op >> 3) & 7) != 7) c.ax = r;
            break; }

        case 0x06: d.push16(c.es); break;
        case 0x07: c.es = d.pop16(); break;
        case 0x0E: d.push16(c.cs); break;
        case 0x16: d.push16(c.ss); break;
        case 0x17: c.ss = d.pop16(); break;
        case 0x1E: d.push16(c.ds); break;
        case 0x1F: c.ds = d.pop16(); break;

        case 0x40: case 0x41: case 0x42: case 0x43:
        case 0x44: case 0x45: case 0x46: case 0x47: {
            uint16_t& r = c.reg16(op & 7);
            uint16_t res = (uint16_t)(r + 1);
            inc_flags(c, r, res, false); r = res; break;
        }
        case 0x48: case 0x49: case 0x4A: case 0x4B:
        case 0x4C: case 0x4D: case 0x4E: case 0x4F: {
            uint16_t& r = c.reg16(op & 7);
            uint16_t res = (uint16_t)(r - 1);
            dec_flags(c, r, res, false); r = res; break;
        }
        case 0x50: case 0x51: case 0x52: case 0x53:
        case 0x54: case 0x55: case 0x56: case 0x57: d.push16(c.reg16(op & 7)); break;
        case 0x58: case 0x59: case 0x5A: case 0x5B:
        case 0x5C: case 0x5D: case 0x5E: case 0x5F: c.reg16(op & 7) = d.pop16(); break;

        case 0x60: { d.push16(c.ax); d.push16(c.cx); d.push16(c.dx); d.push16(c.bx);
            d.push16(c.sp); d.push16(c.bp); d.push16(c.si); d.push16(c.di); break; }
        case 0x61: { c.di = d.pop16(); c.si = d.pop16(); c.bp = d.pop16();
            d.pop16(); c.bx = d.pop16(); c.dx = d.pop16(); c.cx = d.pop16(); c.ax = d.pop16(); break; }
        case 0x62: d.fetch8(); break;
        case 0x68: d.push16(d.fetch16()); break;
        case 0x69: { uint8_t modrm = d.fetch8(); uint16_t b = d.rm_read(modrm >> 6, modrm & 7, false, defseg);
            uint16_t imm = d.fetch16(); int32_t r = (int16_t)b * (int16_t)imm;
            c.flags = (uint16_t)(c.flags & ~(CF | OF));
            if (r > 32767 || r < -32768) c.flags |= CF | OF;
            c.reg16((modrm >> 3) & 7) = (uint16_t)r; break; }
        case 0x6A: { int8_t i = d.fetchs8(); d.push16((uint16_t)(int16_t)i); break; }
        case 0x6B: { uint8_t modrm = d.fetch8(); uint16_t b = d.rm_read(modrm >> 6, modrm & 7, false, defseg);
            int8_t imm = d.fetchs8(); int32_t r = (int16_t)b * imm;
            c.flags = (uint16_t)(c.flags & ~(CF | OF));
            if (r > 32767 || r < -32768) c.flags |= CF | OF;
            c.reg16((modrm >> 3) & 7) = (uint16_t)r; break; }
        case 0x6C: { uint32_t a = ((uint32_t)c.es << 4) + c.di; m.wb(a, m.port_in16(c.dx) & 0xFF);
            c.di += (c.flags & DF) ? -1 : 1; break; }
        case 0x6D: { uint32_t a = ((uint32_t)c.es << 4) + c.di; m.ww(a, m.port_in16(c.dx));
            c.di += (c.flags & DF) ? -2 : 2; break; }
        case 0x6E: { uint32_t a = ((uint32_t)c.ds << 4) + c.si; m.port_out16(c.dx, m.rb(a));
            c.si += (c.flags & DF) ? -1 : 1; break; }
        case 0x6F: { uint32_t a = ((uint32_t)c.ds << 4) + c.si; m.port_out16(c.dx, m.rw(a));
            c.si += (c.flags & DF) ? -2 : 2; break; }

        case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
        case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F: {
            int8_t rel = d.fetchs8();
            if (cond_true(c, op & 0xF)) c.ip = (uint16_t)(c.ip + rel);
            break;
        }
        case 0x80: case 0x81: case 0x82: case 0x83: {
            uint8_t modrm = d.fetch8();
            bool is8 = (op == 0x80 || op == 0x82);
            int aluop = (modrm >> 3) & 7;
            uint8_t mod = modrm >> 6, rm = modrm & 7;
            // resolve the rm address first (this consumes any displacement),
            // then fetch the immediate -- matching the x86 encoding order.
            uint32_t addr; uint16_t cur = d.rm_read2(mod, rm, is8, addr);
            uint16_t imm;
            if (op == 0x83) { int8_t i = d.fetchs8(); imm = (uint16_t)(int16_t)i; }
            else imm = is8 ? d.fetch8() : d.fetch16();
            uint16_t r = alu(c, aluop, cur, imm, is8);
            if (aluop != 7) d.rm_write2(addr, mod, rm, is8, r);
            break;
        }
        case 0x84: { uint8_t modrm = d.fetch8();
            uint32_t addr; uint16_t b = d.rm_read2(modrm >> 6, modrm & 7, true, addr);
            logic_flags(c, (uint16_t)(c.get8((modrm >> 3) & 7) & b), true); break; }
        case 0x85: { uint8_t modrm = d.fetch8();
            uint32_t addr; uint16_t b = d.rm_read2(modrm >> 6, modrm & 7, false, addr);
            logic_flags(c, (uint16_t)(c.reg16((modrm >> 3) & 7) & b), false); break; }
        case 0x86: { uint8_t modrm = d.fetch8();
            uint32_t addr; uint16_t b = d.rm_read2(modrm >> 6, modrm & 7, true, addr);
            uint16_t a = c.get8((modrm >> 3) & 7);
            d.rm_write2(addr, modrm >> 6, modrm & 7, true, a);
            c.set8((modrm >> 3) & 7, (uint8_t)b); break; }
        case 0x87: { uint8_t modrm = d.fetch8();
            uint32_t addr; uint16_t b = d.rm_read2(modrm >> 6, modrm & 7, false, addr);
            uint16_t a = c.reg16((modrm >> 3) & 7);
            d.rm_write2(addr, modrm >> 6, modrm & 7, false, a);
            c.reg16((modrm >> 3) & 7) = b; break; }

        case 0x88: { uint8_t modrm = d.fetch8();
            d.rm_write(modrm >> 6, modrm & 7, true, c.get8((modrm >> 3) & 7), defseg); break; }
        case 0x89: { uint8_t modrm = d.fetch8();
            d.rm_write(modrm >> 6, modrm & 7, false, c.reg16((modrm >> 3) & 7), defseg); break; }
        case 0x8A: { uint8_t modrm = d.fetch8();
            c.set8((modrm >> 3) & 7, (uint8_t)d.rm_read(modrm >> 6, modrm & 7, true, defseg)); break; }
        case 0x8B: { uint8_t modrm = d.fetch8();
            c.reg16((modrm >> 3) & 7) = d.rm_read(modrm >> 6, modrm & 7, false, defseg); break; }
        case 0x8C: { uint8_t modrm = d.fetch8(); int s = (modrm >> 3) & 7;
            uint16_t v = s == 0 ? c.es : s == 1 ? c.cs : s == 2 ? c.ss : s == 3 ? c.ds : 0;
            d.rm_write(modrm >> 6, modrm & 7, false, v, defseg); break; }
        case 0x8D: { uint8_t modrm = d.fetch8();
            uint16_t off = d.ea(modrm >> 6, modrm & 7, defseg);
            c.reg16((modrm >> 3) & 7) = off; break; }
        case 0x8E: { uint8_t modrm = d.fetch8();
            uint16_t v = d.rm_read(modrm >> 6, modrm & 7, false, defseg);
            switch ((modrm >> 3) & 7) { case 0: c.es = v; break; case 1: c.cs = v; break;
                case 2: c.ss = v; break; case 3: c.ds = v; break; } break; }
        case 0x8F: { uint8_t modrm = d.fetch8();
            d.rm_write(modrm >> 6, modrm & 7, false, d.pop16(), defseg); break; }

        case 0x90: break;
        case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97: {
            uint16_t t = c.ax; c.ax = c.reg16(op & 7); c.reg16(op & 7) = t; break; }
        case 0x98: c.ax = (uint16_t)(int16_t)(int8_t)(c.ax & 0xFF); break;
        case 0x99: c.dx = (c.ax & 0x8000) ? 0xFFFF : 0x0000; break;
        case 0x9A: { uint16_t off = d.fetch16(); uint16_t seg = d.fetch16();
            d.push16(c.cs); d.push16(c.ip); c.cs = seg; c.ip = off; break; }
        case 0x9B: break;
        case 0x9C: m.push16(c.flags); break;
        case 0x9D: c.flags = (uint16_t)((m.pop16() & 0x0FD5) | 0x0002); break;
        case 0x9E: { uint8_t ah = (uint8_t)(c.ax >> 8);
            c.flags = (uint16_t)((c.flags & 0xFF00) | (ah & 0xD5) | 2); break; }
        case 0x9F: c.ax = (uint16_t)((c.ax & 0x00FF) | (((c.flags & 0xD5) | 2) << 8)); break;

        case 0xA0: { uint16_t off = d.fetch16(); c.ax = (uint16_t)((c.ax & 0xFF00) |
            m.rb(((uint32_t)d.seg_of(3) << 4) + off)); break; }
        case 0xA1: { uint16_t off = d.fetch16(); c.ax = m.rw(((uint32_t)d.seg_of(3) << 4) + off); break; }
        case 0xA2: { uint16_t off = d.fetch16(); m.wb(((uint32_t)d.seg_of(3) << 4) + off, (uint8_t)c.ax); break; }
        case 0xA3: { uint16_t off = d.fetch16(); m.ww(((uint32_t)d.seg_of(3) << 4) + off, c.ax); break; }

        case 0xA4: case 0xA5: {
            bool w = op == 0xA5;
            uint32_t n = c.rep ? c.cx : 1;
            if (c.rep) c.cx = 0;
            for (uint32_t i = 0; i < n; i++) {
                uint32_t s = ((uint32_t)d.seg_of(3) << 4) + c.si;
                uint32_t t = ((uint32_t)c.es << 4) + c.di;
                if (w) { m.ww(t, m.rw(s)); c.si += (c.flags & DF) ? -2 : 2; c.di += (c.flags & DF) ? -2 : 2; }
                else { m.wb(t, m.rb(s)); c.si += (c.flags & DF) ? -1 : 1; c.di += (c.flags & DF) ? -1 : 1; }
            }
            break;
        }
        case 0xA6: case 0xA7: {
            bool w = op == 0xA7;
            uint32_t n = c.rep ? c.cx : 1;
            for (uint32_t i = 0; i < n; i++) {
                uint32_t s = ((uint32_t)d.seg_of(3) << 4) + c.si;
                uint32_t t = ((uint32_t)c.es << 4) + c.di;
                alu(c, 7, w ? m.rw(s) : m.rb(s), w ? m.rw(t) : m.rb(t), !w);
                if (w) { c.si += (c.flags & DF) ? -2 : 2; c.di += (c.flags & DF) ? -2 : 2; }
                else { c.si += (c.flags & DF) ? -1 : 1; c.di += (c.flags & DF) ? -1 : 1; }
                // REP CMPS leaves the *remaining* count in CX (it is not
                // necessarily zero: an early ZF exit stops the loop).  Code
                // that scans for a terminator relies on this, e.g. the MS-C
                // run-time strlen: "repne scasb; not cx; dec cx".
                if (c.rep) c.cx--;
                if (c.rep && i + 1 < n) {
                    bool cont = (c.rep == 0xF2) ? !(c.flags & ZF) : (c.flags & ZF);
                    if (!cont) break;
                }
            }
            break;
        }
        case 0xA8: logic_flags(c, (uint16_t)((c.ax & 0xFF) & d.fetch8()), true); break;
        case 0xA9: logic_flags(c, (uint16_t)(c.ax & d.fetch16()), false); break;
        case 0xAA: case 0xAB: {
            bool w = op == 0xAB;
            uint32_t n = c.rep ? c.cx : 1;
            if (c.rep) c.cx = 0;
            for (uint32_t i = 0; i < n; i++) {
                uint32_t t = ((uint32_t)c.es << 4) + c.di;
                if (w) { m.ww(t, c.ax); c.di += (c.flags & DF) ? -2 : 2; }
                else { m.wb(t, (uint8_t)c.ax); c.di += (c.flags & DF) ? -1 : 1; }
            }
            break;
        }
        case 0xAC: case 0xAD: {
            bool w = op == 0xAD;
            uint32_t n = c.rep ? c.cx : 1;
            if (c.rep) c.cx = 0;
            for (uint32_t i = 0; i < n; i++) {
                uint32_t s = ((uint32_t)d.seg_of(3) << 4) + c.si;
                c.ax = w ? m.rw(s) : (uint16_t)((c.ax & 0xFF00) | m.rb(s));
                c.si += (c.flags & DF) ? (w ? -2 : -1) : (w ? 2 : 1);
            }
            break;
        }
        case 0xAE: case 0xAF: {
            bool w = op == 0xAF;
            uint32_t n = c.rep ? c.cx : 1;
            for (uint32_t i = 0; i < n; i++) {
                uint32_t t = ((uint32_t)c.es << 4) + c.di;
                alu(c, 7, w ? c.ax : (c.ax & 0xFF), w ? m.rw(t) : m.rb(t), !w);
                c.di += (c.flags & DF) ? (w ? -2 : -1) : (w ? 2 : 1);
                // See the CMPS note above: REP SCAS must leave the remaining
                // count in CX for the run-time's strlen-style scans.
                if (c.rep) c.cx--;
                if (c.rep && i + 1 < n) {
                    bool cont = (c.rep == 0xF2) ? !(c.flags & ZF) : (c.flags & ZF);
                    if (!cont) break;
                }
            }
            break;
        }

        case 0xB0: case 0xB1: case 0xB2: case 0xB3:
        case 0xB4: case 0xB5: case 0xB6: case 0xB7: c.set8(op & 7, d.fetch8()); break;
        case 0xB8: case 0xB9: case 0xBA: case 0xBB:
        case 0xBC: case 0xBD: case 0xBE: case 0xBF: c.reg16(op & 7) = d.fetch16(); break;

        case 0xC0: case 0xC1: {
            uint8_t modrm = d.fetch8(); bool is8 = op == 0xC0;
            uint8_t mod = modrm >> 6, rm = modrm & 7;
            uint32_t addr; uint16_t cur = d.rm_read2(mod, rm, is8, addr);
            uint8_t cnt = d.fetch8();
            uint16_t r = shift_op(c, (modrm >> 3) & 7, cur, cnt, is8);
            d.rm_write2(addr, mod, rm, is8, r);
            break;
        }
        case 0xC2: { uint16_t nn = d.fetch16(); c.ip = d.pop16(); c.sp = (uint16_t)(c.sp + nn); break; }
        case 0xC3: c.ip = d.pop16(); break;
        case 0xC4: { uint8_t modrm = d.fetch8(); int ds2;
            uint16_t off = d.ea(modrm >> 6, modrm & 7, ds2);
            uint32_t a = ((uint32_t)d.seg_of(ds2) << 4) + off;
            c.reg16((modrm >> 3) & 7) = m.rw(a); c.es = m.rw(a + 2); break; }
        case 0xC5: { uint8_t modrm = d.fetch8(); int ds2;
            uint16_t off = d.ea(modrm >> 6, modrm & 7, ds2);
            uint32_t a = ((uint32_t)d.seg_of(ds2) << 4) + off;
            c.reg16((modrm >> 3) & 7) = m.rw(a); c.ds = m.rw(a + 2); break; }
        case 0xC6: { uint8_t modrm = d.fetch8();
            uint8_t mod = modrm >> 6, rm = modrm & 7; int defs = 3;
            if (mod == 3) { c.set8(rm, d.fetch8()); }
            else { uint16_t off = d.ea(mod, rm, defs);
                   m.wb(d.phys(d.seg_of(defs), off), d.fetch8()); }
            break; }
        case 0xC7: { uint8_t modrm = d.fetch8();
            uint8_t mod = modrm >> 6, rm = modrm & 7; int defs = 3;
            if (mod == 3) { c.reg16(rm) = d.fetch16(); }
            else { uint16_t off = d.ea(mod, rm, defs);
                   m.ww(d.phys(d.seg_of(defs), off), d.fetch16()); }
            break; }
        case 0xC8: { uint16_t nn = d.fetch16(); uint8_t lvl = d.fetch8();
            d.push16(c.bp); uint16_t frame = c.sp;
            for (uint8_t i = 1; i < lvl; i++) { c.bp = (uint16_t)(c.bp - 2); d.push16(m.rw(((uint32_t)c.ss << 4) + c.bp)); }
            if (lvl) d.push16(frame);
            c.bp = frame; c.sp = (uint16_t)(c.sp - nn); break; }
        case 0xC9: c.sp = c.bp; c.bp = d.pop16(); break;
        case 0xCA: { uint16_t nn = d.fetch16(); c.ip = d.pop16(); c.cs = d.pop16();
            c.sp = (uint16_t)(c.sp + nn); break; }
        case 0xCB: c.ip = d.pop16(); c.cs = d.pop16(); break;
        case 0xCC: m.push16(c.flags); m.push16(c.cs); m.push16(c.ip);
            c.ip = m.rw(3 * 4); c.cs = m.rw(3 * 4 + 2); c.flags &= ~(IF | TF); break;
        case 0xCD: m.do_int(d.fetch8()); break;
        case 0xCE: if (c.flags & OF) { m.push16(c.flags); m.push16(c.cs); m.push16(c.ip);
            c.ip = m.rw(4 * 4); c.cs = m.rw(4 * 4 + 2); c.flags &= ~(IF | TF); } break;
        case 0xCF: c.ip = d.pop16(); c.cs = d.pop16();
            c.flags = (uint16_t)((d.pop16() & 0x0FD5) | 2); break;

        case 0xD0: case 0xD1: case 0xD2: case 0xD3: {
            uint8_t modrm = d.fetch8(); bool is8 = (op == 0xD0 || op == 0xD2);
            uint8_t cnt = (op >= 0xD2) ? (uint8_t)c.cx : 1;
            uint32_t addr; uint16_t a = d.rm_read2(modrm >> 6, modrm & 7, is8, addr);
            uint16_t r = shift_op(c, (modrm >> 3) & 7, a, cnt, is8);
            d.rm_write2(addr, modrm >> 6, modrm & 7, is8, r); break;
        }
        case 0xD4: { uint8_t b = d.fetch8(); uint8_t al = (uint8_t)(c.ax & 0xFF);
            if (b) c.ax = (uint16_t)(((al / b) << 8) | (al % b));
            break; }
        case 0xD5: { uint8_t b = d.fetch8(); uint8_t al = (uint8_t)(c.ax & 0xFF); uint8_t ah = (uint8_t)(c.ax >> 8);
            c.ax = (uint16_t)((ah * b + al) & 0xFF); break; }
        case 0xD7: { uint32_t a = ((uint32_t)d.seg_of(3) << 4) + c.bx;
            c.ax = (uint16_t)((c.ax & 0xFF00) | m.rb(a)); break; }
        case 0xD8: case 0xD9: case 0xDA: case 0xDB: case 0xDC: case 0xDD: case 0xDE: case 0xDF:
            break;
        case 0xE0: { int8_t rel = d.fetchs8(); c.cx--; if (c.cx && !(c.flags & ZF)) c.ip = (uint16_t)(c.ip + rel); break; }
        case 0xE1: { int8_t rel = d.fetchs8(); c.cx--; if (c.cx && (c.flags & ZF)) c.ip = (uint16_t)(c.ip + rel); break; }
        case 0xE2: { int8_t rel = d.fetchs8(); c.cx--; if (c.cx) c.ip = (uint16_t)(c.ip + rel); break; }
        case 0xE3: { int8_t rel = d.fetchs8(); if (c.cx == 0) c.ip = (uint16_t)(c.ip + rel); break; }
        case 0xE4: { uint8_t p = d.fetch8(); c.ax = (uint16_t)((c.ax & 0xFF00) | (m.port_in16(p) & 0xFF)); break; }
        case 0xE5: { uint8_t p = d.fetch8(); c.ax = m.port_in16(p); break; }
        case 0xE6: { uint8_t p = d.fetch8(); m.port_out16(p, c.ax & 0xFF); break; }
        case 0xE7: { uint8_t p = d.fetch8(); m.port_out16(p, c.ax); break; }
        case 0xE8: { uint16_t rel = d.fetch16(); d.push16(c.ip); c.ip = (uint16_t)(c.ip + rel); break; }
        case 0xE9: { uint16_t rel = d.fetch16(); c.ip = (uint16_t)(c.ip + rel); break; }
        case 0xEA: { uint16_t off = d.fetch16(); uint16_t seg = d.fetch16(); c.ip = off; c.cs = seg; break; }
        case 0xEB: { int8_t rel = d.fetchs8(); c.ip = (uint16_t)(c.ip + rel); break; }
        case 0xEC: c.ax = (uint16_t)((c.ax & 0xFF00) | (m.port_in16(c.dx) & 0xFF)); break;
        case 0xED: c.ax = m.port_in16(c.dx); break;
        case 0xEE: m.port_out16(c.dx, c.ax & 0xFF); break;
        case 0xEF: m.port_out16(c.dx, c.ax); break;

        case 0xF4: c.halted = true; break;
        case 0xF5: c.flags ^= CF; break;
        case 0xF6: case 0xF7: {
            uint8_t modrm = d.fetch8(); bool is8 = op == 0xF6;
            uint32_t addr; uint16_t a = d.rm_read2(modrm >> 6, modrm & 7, is8, addr);
            switch ((modrm >> 3) & 7) {
                case 0: case 1: logic_flags(c, (uint16_t)(a & (is8 ? d.fetch8() : d.fetch16())), is8); break;
                case 2: { uint16_t r = is8 ? (uint16_t)(~(uint8_t)a) : (uint16_t)~a;
                    d.rm_write2(addr, modrm >> 6, modrm & 7, is8, r); break; }
                case 3: { uint16_t r = is8 ? (uint16_t)(-(int8_t)a) : (uint16_t)(-(int16_t)a);
                    uint16_t f = (uint16_t)(c.flags & ~(CF | OF | SF | ZF | PF | AF));
                    if (r) f |= CF;
                    if (r & (is8 ? 0x80 : 0x8000)) f |= SF;
                    if (r == 0) f |= ZF;
                    f |= pf_of((uint8_t)r); c.flags = f;
                    d.rm_write2(addr, modrm >> 6, modrm & 7, is8, r); break; }
                case 4: { if (is8) { uint16_t r = (uint16_t)((c.ax & 0xFF) * (uint8_t)a); c.ax = r;
                        c.flags = (uint16_t)(c.flags & ~(CF | OF)); if (r > 0xFF) c.flags |= CF | OF; }
                    else { uint32_t r = (uint32_t)c.ax * a; c.ax = (uint16_t)r; c.dx = (uint16_t)(r >> 16);
                        c.flags = (uint16_t)(c.flags & ~(CF | OF)); if (c.dx) c.flags |= CF | OF; } break; }
                case 5: { if (is8) { int16_t r = (int16_t)((int8_t)(c.ax & 0xFF) * (int8_t)a); c.ax = (uint16_t)r;
                        c.flags = (uint16_t)(c.flags & ~(CF | OF)); if (r > 127 || r < -128) c.flags |= CF | OF; }
                    else { int32_t r = (int32_t)((int16_t)c.ax * (int16_t)a); c.ax = (uint16_t)r; c.dx = (uint16_t)(r >> 16);
                        c.flags = (uint16_t)(c.flags & ~(CF | OF)); if (r > 32767 || r < -32768) c.flags |= CF | OF; } break; }
                case 6: { if (is8) { uint16_t num = c.ax; uint8_t dv = (uint8_t)(a & 0xFF);
                        if (dv == 0) { m.push16(c.flags); m.push16(c.cs); m.push16(c.ip); c.ip = m.rw(0); c.cs = m.rw(2); c.flags &= ~(IF | TF); }
                        else c.ax = (uint16_t)(((num % dv) << 8) | (num / dv)); }
                    else { uint32_t num = ((uint32_t)c.dx << 16) | c.ax;
                        if (a == 0) { m.push16(c.flags); m.push16(c.cs); m.push16(c.ip); c.ip = m.rw(0); c.cs = m.rw(2); c.flags &= ~(IF | TF); }
                        else { c.ax = (uint16_t)(num / a); c.dx = (uint16_t)(num % a); } } break; }
                default: { if (is8) { int16_t num = (int16_t)c.ax; int8_t dv = (int8_t)a;
                        if (dv == 0) { m.push16(c.flags); m.push16(c.cs); m.push16(c.ip); c.ip = m.rw(0); c.cs = m.rw(2); c.flags &= ~(IF | TF); }
                        else c.ax = (uint16_t)(((num % dv) << 8) | ((num / dv) & 0xFF)); }
                    else { int32_t num = ((int32_t)c.dx << 16) | c.ax;
                        if (a == 0) { m.push16(c.flags); m.push16(c.cs); m.push16(c.ip); c.ip = m.rw(0); c.cs = m.rw(2); c.flags &= ~(IF | TF); }
                        else { c.ax = (uint16_t)((num / (int16_t)a) & 0xFFFF); c.dx = (uint16_t)(num % (int16_t)a); } } break; }
            }
            break;
        }
        case 0xF8: c.flags &= ~CF; break;
        case 0xF9: c.flags |= CF; break;
        case 0xFA: c.flags &= ~IF; break;
        case 0xFB: c.flags |= IF; break;
        case 0xFC: c.flags &= ~DF; break;
        case 0xFD: c.flags |= DF; break;
        case 0xFE: {
            uint8_t modrm = d.fetch8();
            uint32_t addr; uint16_t a = d.rm_read2(modrm >> 6, modrm & 7, true, addr);
            if (((modrm >> 3) & 7) == 0) { uint16_t r = (uint16_t)((a + 1) & 0xFF); inc_flags(c, a, r, true);
                d.rm_write2(addr, modrm >> 6, modrm & 7, true, r); }
            else { uint16_t r = (uint16_t)((a - 1) & 0xFF); dec_flags(c, a, r, true);
                d.rm_write2(addr, modrm >> 6, modrm & 7, true, r); }
            break;
        }
        case 0xFF: {
            uint8_t modrm = d.fetch8(); int sub = (modrm >> 3) & 7;
            switch (sub) {
                case 0: { uint32_t addr; uint16_t a = d.rm_read2(modrm >> 6, modrm & 7, false, addr);
                    uint16_t r = (uint16_t)(a + 1); inc_flags(c, a, r, false);
                    d.rm_write2(addr, modrm >> 6, modrm & 7, false, r); break; }
                case 1: { uint32_t addr; uint16_t a = d.rm_read2(modrm >> 6, modrm & 7, false, addr);
                    uint16_t r = (uint16_t)(a - 1); dec_flags(c, a, r, false);
                    d.rm_write2(addr, modrm >> 6, modrm & 7, false, r); break; }
                case 2: { uint16_t t = d.rm_read(modrm >> 6, modrm & 7, false, defseg);
                    d.push16(c.ip); c.ip = t; break; }
                case 3: { int ds2; uint16_t off = d.ea(modrm >> 6, modrm & 7, ds2);
                    uint32_t a = ((uint32_t)d.seg_of(ds2) << 4) + off;
                    uint16_t noff = m.rw(a), nseg = m.rw(a + 2);
                    d.push16(c.cs); d.push16(c.ip); c.cs = nseg; c.ip = noff; break; }
                case 4: { uint16_t t = d.rm_read(modrm >> 6, modrm & 7, false, defseg); c.ip = t; break; }
                case 5: { int ds2; uint16_t off = d.ea(modrm >> 6, modrm & 7, ds2);
                    uint32_t a = ((uint32_t)d.seg_of(ds2) << 4) + off;
                    c.ip = m.rw(a); c.cs = m.rw(a + 2); break; }
                case 6: d.push16(d.rm_read(modrm >> 6, modrm & 7, false, defseg)); break;
                default: break;
            }
            break;
        }
        case 0x0F: d.fetch8(); break;
        default:
            if (getenv("LOF_UNK")) fprintf(stderr, "UNKNOWN OP %02X at %04X:%04X ax=%04X bx=%04X cx=%04X dx=%04X si=%04X di=%04X ds=%04X es=%04X ss=%04X sp=%04X\n", op, c.cs, (uint16_t)(c.ip-1), c.ax,c.bx,c.cx,c.dx,c.si,c.di,c.ds,c.es,c.ss,c.sp);
            m.running = false;
            break;
    }
}

}  // namespace lof
