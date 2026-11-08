// Native x86/DOS/EGA environment for Legend of Faerghail (reLINE, 1990).
//
// The game ships as 16-bit real-mode MZ images (LOF.EXE, VOR.EXE) built with
// the 1988 Microsoft C run-time.  Rather than re-implement the engine from
// scratch, we execute the original code faithfully: an 8086 interpreter plus
// the hardware it actually touches (EGA, 8259/8253, keyboard, mouse) and the
// DOS/BIOS services the MS-C run-time calls.
#ifndef LOF_EMU_H
#define LOF_EMU_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace lof {

// ---------------------------------------------------------------- flags ---
enum {
    CF = 0x0001, PF = 0x0004, AF = 0x0010, ZF = 0x0040,
    SF = 0x0080, TF = 0x0100, IF = 0x0200, DF = 0x0400, OF = 0x0800,
};

struct Machine;

struct CPU {
    uint16_t ax = 0, cx = 0, dx = 0, bx = 0;
    uint16_t sp = 0, bp = 0, si = 0, di = 0;
    uint16_t es = 0, cs = 0, ss = 0, ds = 0;
    uint16_t ip = 0;
    uint16_t flags = 0x0002;   // reserved bit 1 always set
    bool halted = false;
    bool trace = false;

    // segment override prefix state
    bool has_ovr = false;
    uint16_t ovr_seg = 0;
    uint8_t rep = 0;           // 0, 0xF2 (REPNZ), 0xF3 (REP/REPZ)

    Machine* m = nullptr;

    uint16_t& reg16(int i) {
        switch (i) {
            case 0: return ax; case 1: return cx; case 2: return dx; case 3: return bx;
            case 4: return sp; case 5: return bp; case 6: return si; default: return di;
        }
    }
    uint8_t& reg8(int i) {
        // 0=AL 1=CL 2=DL 3=BL 4=AH 5=CH 6=DH 7=BH
        static uint8_t hi;  // never used directly; handled by caller via 16-bit reg
        return hi;
    }
    uint8_t get8(int i);
    void set8(int i, uint8_t v);
};

// ------------------------------------------------------------ EGA video ---
struct Ega {
    // 4 planes, 64 KiB each (256 KiB)
    std::vector<uint8_t> plane[4];
    // sequencer
    uint8_t seq_index = 0;
    uint8_t seq_map_mask = 0x0F;
    uint8_t seq_mem_mode = 0;
    // graphics controller
    uint8_t gc_index = 0;
    uint8_t gc_setres = 0;
    uint8_t gc_enable_setres = 0;
    uint8_t gc_comp = 0;
    uint8_t gc_rot = 0;
    uint8_t gc_read_map = 0;
    uint8_t gc_mode = 0;
    uint8_t gc_misc = 0;
    uint8_t gc_bitmask = 0xFF;
    uint8_t gc_xor = 0;
    uint8_t gc_latch[4] = {0, 0, 0, 0};
    bool gc_latch_valid = false;
    // CRTC
    uint8_t crtc_index = 0;
    uint8_t crtc[32] = {0};
    uint16_t crtc_start = 0;
    // attribute controller
    uint8_t attr_index = 0;
    bool attr_flip = false;   // toggles between index and data writes
    uint8_t attr_pal[16] = {0};
    uint8_t attr_overscan = 0;
    uint8_t attr_color_select = 0;

    int mode = 3;             // current BIOS video mode
    uint64_t writes = 0;      // diagnostic: number of CPU writes into VRAM

    void reset();
    int width() const;        // active pixel width
    int height() const;       // active pixel height
    int bpp() const;          // bits per pixel (1 or 4)
    bool graphics() const { return mode == 0x0D || mode == 0x10 || mode == 0x0E || mode == 0x12; }

    // planar CPU access
    void write(uint32_t off, uint8_t v);
    uint8_t read(uint32_t off);
    uint8_t read_plane(uint32_t off, int plane);

    void set_default_palette();
};

// ------------------------------------------------------------- machine ---
struct Machine {
    std::vector<uint8_t> ram;          // 1 MiB + 64 KiB
    CPU cpu;
    Ega ega;

    std::string game_dir;              // host directory mapped to the DOS C: drive

    // DOS state
    uint16_t psp_seg = 0;
    uint16_t alloc_top = 0;            // first free paragraph
    bool running = true;
    int exit_code = 0;

    // keyboard
    std::vector<uint8_t> key_queue;    // scancodes
    uint8_t kbd_shift = 0;

    // mouse
    int mouse_x = 320, mouse_y = 175;
    uint8_t mouse_buttons = 0;
    bool mouse_visible = false;
    int mouse_minx = 0, mouse_maxx = 639, mouse_miny = 0, mouse_maxy = 199;
    uint8_t mouse_last_buttons = 0;

    // timer
    uint32_t ticks = 0;                // 18.2 Hz BIOS ticks
    uint32_t count_21 = 0, count_10 = 0, count_16 = 0;
    uint32_t instr_count = 0;

    // video framebuffer (rendered each frame)
    std::vector<uint32_t> framebuffer;
    int fb_w = 640, fb_h = 350;
    bool fb_dirty = true;

    // called after each burst of instructions (input pump / frame pacing)
    std::function<void()> pump;

    // ---- memory access ----
    uint8_t* mem() { return ram.data(); }
    inline bool is_ega(uint32_t a) {
        return ega.graphics() && a >= 0xA0000 && a < 0xB0000;
    }
    inline uint8_t rb(uint32_t a) {
        if (is_ega(a)) return ega.read(a - 0xA0000);
        return ram[a & 0xFFFFF];
    }
    inline uint16_t rw(uint32_t a) {
        if (is_ega(a)) { uint8_t lo = ega.read(a - 0xA0000); return (uint16_t)(lo | (ega.read(a + 1 - 0xA0000) << 8)); }
        a &= 0xFFFFF;
        return (uint16_t)(ram[a] | (ram[(a + 1) & 0xFFFFF] << 8));
    }
    inline void wb(uint32_t a, uint8_t v) {
        if (is_ega(a)) { if(getenv("LOF_VC")){ static unsigned long key[8192]; static unsigned cnt[8192],lo[8192],hi[8192]; static int nk=0; static bool reg=false; if(!reg){reg=true; atexit([]{ for(int i=0;i<nk;i++) if(cnt[i]>200) fprintf(stderr,"[vc] %04X:%04X n=%u off=%04X..%04X\n",(unsigned)(key[i]>>16),(unsigned)(key[i]&0xFFFF),cnt[i],lo[i],hi[i]); });} uint32_t k=((uint32_t)cpu.cs<<16)|cpu.ip; int f=-1; for(int i=0;i<nk;i++) if(key[i]==k){f=i;break;} if(f<0&&nk<8192){ f=nk; key[nk]=k; cnt[nk]=0; lo[nk]=0xFFFF; hi[nk]=0; nk++; } if(f>=0){ cnt[f]++; uint32_t o=a-0xA0000; if(o<lo[f])lo[f]=o; if(o>hi[f])hi[f]=o; } } ega.write(a - 0xA0000, v); fb_dirty = true; return; }
        ram[a & 0xFFFFF] = v;
    }
    inline void ww(uint32_t a, uint16_t v) {
        if (is_ega(a)) { ega.write(a - 0xA0000, (uint8_t)v); ega.write(a + 1 - 0xA0000, (uint8_t)(v >> 8)); fb_dirty = true; return; }
        a &= 0xFFFFF;
        ram[a] = v & 0xFF; ram[(a + 1) & 0xFFFFF] = v >> 8;
    }

    // ---- CPU plumbing ----
    void reset_cpu_state();
    void load_exe(const std::vector<uint8_t>& image, const std::string& name);
    void run();

    // ---- services ----
    void do_int(uint8_t n);
    void bios_int(uint8_t n);
    void dos_int(uint8_t n);
    uint8_t port_in16(uint16_t p);
    void port_out16(uint16_t p, uint16_t v);
    void port_write_byte(uint16_t p, uint8_t b);

    void push16(uint16_t v);
    uint16_t pop16();
    void pushf();
    void popf();

    void render();
    void set_video_mode(int mode);

    // DOS helpers
    std::string resolve(const std::string& dos_path);
    void dos_terminate(int code);
};

// CPU execution
void cpu_step(CPU& c);

}  // namespace lof

#endif
