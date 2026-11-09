// Self-tests for the native x86/DOS/EGA environment.
//
// These exercise the pieces that are easy to get subtly wrong and that the
// game's MS-C run-time depends on: the 8086 ALU/addressing core, the
// scan-code -> ASCII translation used by the BIOS/DOS keyboard services, and
// the EGA plane/mask write path.  They run without any game data.
#include "emu.h"

#include <cstdio>
#include <cstring>

using namespace lof;

static int g_fail = 0;

#define CHECK(cond) do { if (!(cond)) { \
    std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); g_fail++; } } while (0)

static void test_alu() {
    Machine m;
    m.reset_cpu_state();
    // mov ax,1234h ; mov bx,5678h ; add ax,bx  => 68ACh
    const uint8_t code[] = {
        0xB8, 0x34, 0x12,
        0xBB, 0x78, 0x56,
        0x01, 0xD8,
        0xF4,                       // hlt
    };
    std::memcpy(&m.ram[0x1000 << 4], code, sizeof code);
    m.cpu.cs = 0x1000;
    m.cpu.ip = 0;
    for (int i = 0; i < 8 && !m.cpu.halted; i++) cpu_step(m.cpu);
    CHECK(m.cpu.ax == 0x68AC);
    CHECK(m.cpu.bx == 0x5678);

    // cmp ax,ax => ZF set
    m.reset_cpu_state();
    m.cpu.ax = 0x0042;
    const uint8_t code2[] = { 0x39, 0xC0, 0xF4 };
    std::memcpy(&m.ram[0x1000 << 4], code2, sizeof code2);
    m.cpu.cs = 0x1000; m.cpu.ip = 0;
    for (int i = 0; i < 4 && !m.cpu.halted; i++) cpu_step(m.cpu);
    CHECK(m.cpu.flags & ZF);
}

static void test_keyboard() {
    struct { uint8_t sc; uint8_t want; } t[] = {
        {0x1C, '\r'}, {0x39, ' '}, {0x01, 0x1B}, {0x0E, 0x08}, {0x0F, 0x09},
        {0x1E, 'a'}, {0x2C, 'z'}, {0x2D, 'x'}, {0x35, '/'}, {0x10, 'q'},
        {0x02, '1'}, {0x0D, '='}, {0x29, '`'}, {0x2B, '\\'}, {0x1A, '['},
        {0x1B, ']'}, {0x28, '\''}, {0x27, ';'},
    };
    for (auto& x : t) CHECK(scancode_ascii(x.sc) == x.want);
}

static void test_ega_write() {
    Machine m;
    m.reset_cpu_state();
    // Enter EGA graphics mode (INT 10h AH=00 AL=0Dh), set map mask = 0x01,
    // then verify a write through A000:0000 only touches plane 0.
    m.cpu.ax = 0x000D;
    m.do_int(0x10);
    m.port_write_byte(0x3C4, 0x02);
    m.port_write_byte(0x3C5, 0x01);
    m.port_write_byte(0x3CE, 0x01); m.port_write_byte(0x3CF, 0x00);
    m.port_write_byte(0x3CE, 0x08); m.port_write_byte(0x3CF, 0xFF);
    m.port_write_byte(0x3CE, 0x05); m.port_write_byte(0x3CF, 0x00);
    m.wb(0xA0000, 0x5A);
    CHECK(m.ega.plane[0][0] == 0x5A);
    CHECK(m.ega.plane[1][0] == 0x00);
    CHECK(m.ega.plane[2][0] == 0x00);
    CHECK(m.ega.plane[3][0] == 0x00);
}

static void test_string_rep() {
    // The MS-C run-time's strlen is "repne scasb; not cx; dec cx".  REP SCAS
    // (and CMPS) must leave the *remaining* count in CX, not force it to 0,
    // otherwise the length is garbage (0xFFFE) and every %s/%d width that
    // depends on it mis-pads.  Scan "abc\0" in ES:DI with AL=0.
    Machine m;
    m.reset_cpu_state();
    const uint8_t buf[] = {'a', 'b', 'c', 0};
    std::memcpy(&m.ram[0x2000 << 4], buf, sizeof buf);
    m.cpu.es = 0x2000; m.cpu.di = 0;
    m.cpu.ax = 0x0000;                 // AL = 0 (search value)
    m.cpu.cx = 0xFFFF;
    m.cpu.flags &= ~DF;                // forward
    const uint8_t code[] = { 0xF2, 0xAE, 0xF4 };   // repne scasb; hlt
    std::memcpy(&m.ram[0x1000 << 4], code, sizeof code);
    m.cpu.cs = 0x1000; m.cpu.ip = 0;
    for (int i = 0; i < 12 && !m.cpu.halted; i++) cpu_step(m.cpu);
    // 4 bytes scanned (a, b, c, NUL) -> CX = 0xFFFF - 4 = 0xFFFB, and
    // strlen = ~CX - 1 = 4 - 1 = 3.
    CHECK(m.cpu.cx == 0xFFFB);
    uint16_t len = (uint16_t)(~m.cpu.cx);
    CHECK((uint16_t)(len - 1) == 3);

    // REPE CMPS stops at the first difference, leaving CX = remaining count.
    m.reset_cpu_state();
    const uint8_t s1[] = {'x', 'y', 'z'};
    const uint8_t s2[] = {'x', 'y', 'w'};
    std::memcpy(&m.ram[0x2000 << 4], s1, sizeof s1);
    std::memcpy(&m.ram[0x3000 << 4], s2, sizeof s2);
    m.cpu.ds = 0x2000; m.cpu.si = 0;
    m.cpu.es = 0x3000; m.cpu.di = 0;
    m.cpu.cx = 0x0005;
    m.cpu.flags &= ~DF;
    const uint8_t code2[] = { 0xF3, 0xA6, 0xF4 };  // repe cmpsb; hlt
    std::memcpy(&m.ram[0x1000 << 4], code2, sizeof code2);
    m.cpu.cs = 0x1000; m.cpu.ip = 0;
    for (int i = 0; i < 12 && !m.cpu.halted; i++) cpu_step(m.cpu);
    CHECK(m.cpu.cx == 2);              // 3 bytes consumed of 5
    CHECK(!(m.cpu.flags & ZF));
}

int main() {
    test_alu();
    test_keyboard();
    test_ega_write();
    test_string_rep();
    if (g_fail == 0) { std::printf("all tests passed\n"); return 0; }
    std::printf("%d test(s) failed\n", g_fail);
    return 1;
}
