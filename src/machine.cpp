// Machine: memory map, MZ loader, DOS/BIOS services and the main run loop.
#include "emu.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <functional>
#include <map>

#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

namespace lof {

static const uint16_t kPspSeg = 0x1000;

static void klog(const char* who, uint8_t sc) {
    if (getenv("LOF_KEYLOG")) fprintf(stderr, "[keylog] %s consumed sc=%02X ('%c')\n", who, sc, sc<128 && sc>=32 ? sc : '?');
}

static void klog2(const char* who, uint8_t sc, uint32_t ic) {
    if (getenv("LOF_KEYLOG")) fprintf(stderr, "[keylog] %s consumed sc=%02X ('%c') instr=%u\n", who, sc, sc<128 && sc>=32 ? sc : '?', ic);
}

// ---------------------------------------------------------------- helpers ---
[[maybe_unused]] static std::string upper(std::string s) {
    for (char& c : s) c = (char)toupper((unsigned char)c);
    return s;
}
static std::string dos_to_host(std::string p) {
    for (char& c : p) if (c == '\\') c = '/';
    return p;
}
[[maybe_unused]] static bool is_abs(const std::string& p) {
    return !p.empty() && (p[0] == '/' || p[0] == '\\' || (p.size() > 1 && p[1] == ':'));
}

// ------------------------------------------------------------------ ctor ---
void Machine::reset_cpu_state(bool /*wipe*/) {
    ram.assign(0x110000, 0);
    ega.reset();
    cpu.m = this;
    cpu.cs = kPspSeg; cpu.ip = 0;
    cpu.ss = kPspSeg; cpu.sp = 0xFFFE;
    cpu.ds = kPspSeg; cpu.es = kPspSeg;
    cpu.flags = 0x0202;
    cpu.halted = false;
    cpu.has_ovr = false;
    cpu.rep = 0;
    psp_seg = kPspSeg;
    alloc_top = 0xA000;
    // BIOS data area basics
    ww(0x410, 0x0000);           // equipment
    ww(0x413, 640 - 64);         // KB memory size
    ww(0x41A, 0x001E);           // kbd buffer head
    ww(0x41C, 0x001E);           // kbd buffer tail
    wb(0x449, 0x03);             // video mode
    ww(0x44A, 80);               // columns
    wb(0x484, 24);               // rows-1
    ww(0x44C, 4000);             // regen size (text)
    wb(0x462, 0);
    ww(0x463, 0x03D4);           // CRTC base
    // interrupt vectors: point INT 20h..INT 3Fh at a tiny handler (IRET)
    // We dispatch interrupts directly in do_int, so vectors are mostly cosmetic.
}

void Machine::set_video_mode(int mode) {
    if (getenv("LOF_MODE")) fprintf(stderr,"[mode] set_video_mode(%02X) from cs:ip=%04X:%04X\n", mode, cpu.cs, cpu.ip);
    ega.mode = mode;
    ega.set_default_palette();
    if (ega.graphics()) { ega.attr_pal[0] = 0; for (int i = 0; i < 16; i++) ega.attr_pal[i] = (uint8_t)i; }
    wb(0x449, (uint8_t)mode);
    if (mode == 3 || mode == 2 || mode == 7) {
        ww(0x44C, 4000); ww(0x44A, 80); wb(0x484, 24);
    } else {
        ww(0x44C, 0x4000); ww(0x44A, 40); wb(0x484, 24);
    }
    fb_dirty = true;
}

// --------------------------------------------------------------- ports -----
uint8_t Machine::port_in16(uint16_t p) {
    switch (p) {
        case 0x3C0: return ega.attr_index | (ega.attr_flip ? 0x20 : 0);
        case 0x3C1: return ega.attr_pal[ega.attr_index & 0x0F];
        case 0x3C4: return ega.seq_index;
        case 0x3C5: switch (ega.seq_index) { case 2: return ega.seq_map_mask; case 4: return ega.seq_mem_mode; default: return 0; }
        case 0x3CE: return ega.gc_index;
        case 0x3CF: switch (ega.gc_index) {
            case 0: return ega.gc_setres; case 1: return ega.gc_enable_setres;
            case 2: return ega.attr_color_select; case 3: return ega.gc_rot;
            case 4: return ega.gc_read_map; case 5: return ega.gc_mode;
            case 6: return ega.gc_misc; case 7: return ega.gc_read_map;
            case 8: return ega.gc_bitmask; default: return ega.gc_xor; }
        case 0x3D4: return ega.crtc_index;
        case 0x3D5: return ega.crtc[ega.crtc_index & 0x1F];
        case 0x3DA: {  // input status 1: alternate retrace bits
            static bool t = false; t = !t;
            return (uint8_t)((t ? 0x01 : 0x00) | 0x08);
        }
        case 0x60: case 0x64: return 0;
        case 0x40: case 0x41: case 0x42: case 0x43: return 0;
        case 0x61: return 0x20;
        default: return 0xFF;
    }
}

void Machine::port_out16(uint16_t p, uint16_t v) {
    if (getenv("LOF_PTRACE") && (p==0x3C0||p==0x3C4||p==0x3C5||p==0x3CE||p==0x3CF||p==0x3D4||p==0x3D5)) {
        static int n=0; if(n<80){ fprintf(stderr,"OUT %03X=%02X (hi=%02X) cs:ip=%04X:%04X\n",p,v&0xFF,(v>>8)&0xFF,cpu.cs,(uint16_t)(cpu.ip-2)); n++; }
    }
    port_write_byte(p, (uint8_t)(v & 0xFF));
    // A 16-bit OUT to an index port also drives the paired data port with AH.
    // EGA/VGA code commonly uses `mov al,idx; mov ah,data; out dx,ax`.
    if (v & 0xFF00) {
        switch (p) {
            case 0x3C0:
                // Attribute controller: index and data both go to 0x3C0.
                port_write_byte(0x3C0, (uint8_t)(v >> 8));
                break;
            case 0x3C4: case 0x3CE: case 0x3D4:
                port_write_byte((uint16_t)(p + 1), (uint8_t)(v >> 8));
                break;
            default: break;
        }
    }
}

void Machine::port_write_byte(uint16_t p, uint8_t b) {
    uint16_t v = b;
    switch (p) {
        case 0x3C0:
            if (getenv("LOF_ATTR")) { static int n=0; if(n<120){ fprintf(stderr,"[3C0] %s val=%02X cs:ip=%04X:%04X\n", ega.attr_flip?"DATA":"IDX ", b, cpu.cs, cpu.ip); n++; } }
            if (!ega.attr_flip) { ega.attr_index = v & 0x1F; ega.attr_flip = true; }
            else {
                if ((ega.attr_index & 0x1F) < 16) ega.attr_pal[ega.attr_index & 0x0F] = v & 0x3F;
                else if ((ega.attr_index & 0x1F) == 0x10) ega.attr_overscan = v & 0x3F;
                else if ((ega.attr_index & 0x1F) == 0x12) ega.attr_color_select = v & 0x0F;
                ega.attr_flip = false;
            }
            fb_dirty = true; break;
        case 0x3C2: ega.attr_flip = false; break;
        case 0x3C4: ega.seq_index = v & 0x1F; break;
        case 0x3C5:
            switch (ega.seq_index) {
                case 2: ega.seq_map_mask = v & 0x0F; break;
                case 4: ega.seq_mem_mode = v & 0x08; break;
            }
            break;
        case 0x3CE: ega.gc_index = v & 0x1F;
            if (getenv("LOF_GC")) { static int n=0; if(n<60){ fprintf(stderr,"[gc] idx=%u\n", v&0x1F); n++; } }
            break;
        case 0x3CF:
            if (getenv("LOF_GC")) { static int n=0; if(n<60){ fprintf(stderr,"[gc]   val=%02X (idx=%u)\n", b, ega.gc_index); n++; } }
            switch (ega.gc_index) {
                case 0: ega.gc_setres = v & 0xFF; break;
                case 1: ega.gc_enable_setres = v & 0x0F; break;
                case 2: ega.attr_color_select = v & 0x0F; break;
                case 3: ega.gc_rot = v & 0x1F; break;
                case 4: ega.gc_read_map = v & 0x03; break;
                case 5: ega.gc_mode = v & 0x1F; break;
                case 6: ega.gc_misc = v & 0x0F; break;
                case 7: ega.gc_read_map = v & 0x03; break;   // read map select
                case 8: if (getenv("LOF_BM")) { static int n=0; if(n<60){ fprintf(stderr,"[bm] val=%02X cs:ip=%04X:%04X instr=%u\n", b, cpu.cs, cpu.ip, instr_count); n++; } } ega.gc_bitmask = v & 0xFF; break;
            }
            break;
        case 0x3D4: ega.crtc_index = v & 0x1F; break;
        case 0x3D5:
            if (getenv("LOF_CRTC")) { static int n=0; if(n<200){ fprintf(stderr,"[crtc] idx=%02X val=%02X cs:ip=%04X:%04X\n", ega.crtc_index&0x1F, v, cpu.cs, cpu.ip); n++; } }
            if ((ega.crtc_index & 0x1F) < 32) ega.crtc[ega.crtc_index & 0x1F] = v & 0xFF;
            break;
        case 0x3C7: case 0x3C8: case 0x3C9: break;   // DAC (unused on EGA)
        case 0x60: case 0x61: case 0x64: break;
        case 0x40: case 0x41: case 0x42: case 0x43: break;
        default: break;
    }
}

// --------------------------------------------------------------- stack -----
void Machine::push16(uint16_t v) { cpu.sp -= 2; ww(((uint32_t)cpu.ss << 4) + cpu.sp, v); }
uint16_t Machine::pop16() { uint16_t v = rw(((uint32_t)cpu.ss << 4) + cpu.sp); cpu.sp += 2; return v; }
void Machine::pushf() { push16(cpu.flags); }
void Machine::popf() { cpu.flags = (uint16_t)((pop16() & 0x0FD5) | 2); }

// --------------------------------------------------------------- MZ load ---
void Machine::load_exe(const std::vector<uint8_t>& img, const std::string& name) {
    (void)name;
    if (img.size() < 28 || img[0] != 'M' || img[1] != 'Z') { running = false; return; }
    auto g16 = [&](size_t o) { return (uint16_t)(img[o] | (img[o + 1] << 8)); };
    uint16_t nreloc = g16(6), hdrsz = g16(8), minalloc = g16(10);
    uint16_t ss = g16(14), sp = g16(16), ip = g16(20), cs = g16(22), reltbl = g16(24);
    size_t hdr = (size_t)hdrsz * 16;
    size_t imglen = img.size() - hdr;
    uint16_t loadseg = kPspSeg + 0x10;   // PSP is 0x100 bytes = 16 paragraphs
    uint32_t base = (uint32_t)loadseg << 4;
    std::memcpy(&ram[base], &img[hdr], std::min(imglen, ram.size() - base));
    // First free paragraph after the loaded image, rounded up to a paragraph.
    alloc_free = (uint16_t)(loadseg + ((imglen + 15) >> 4) + 1);
    // apply relocations
    for (uint16_t i = 0; i < nreloc; i++) {
        size_t o = reltbl + (size_t)i * 4;
        if (o + 4 > img.size()) break;
        uint16_t off = g16(o), seg = g16(o + 2);
        uint32_t a = base + ((uint32_t)seg << 4) + off;
        uint16_t v = rw(a);
        ww(a, (uint16_t)(v + loadseg));
    }
    // LOF's startup guard aborts unless the copy-protection signature "lofx" is
    // present in the INT 0xFA vector slot.  A legitimate install has VOR (run
    // first by START.BAT) plant it, so plant it here too when loading LOF.  We
    // must NOT plant it before VOR itself, which uses the same slot to detect an
    // already-resident instance ("Please reboot your system without resident
    // programs.").
    {
        std::string lower;
        for (char c : name) lower += (char)tolower((unsigned char)c);
        if (lower.find("vor") == std::string::npos) {
            wb(0x3E8, 'l'); wb(0x3E9, 'o'); wb(0x3EA, 'f'); wb(0x3EB, 'x');
        }
    }
    cpu.cs = (uint16_t)(loadseg + cs);
    cpu.ip = ip;
    cpu.ss = (uint16_t)(loadseg + ss);
    cpu.sp = sp;
    cpu.ds = kPspSeg;
    cpu.es = kPspSeg;
    // build a minimal PSP
    std::memset(&ram[(uint32_t)kPspSeg << 4], 0, 0x100);
    ww(((uint32_t)kPspSeg << 4) + 0x00, 0x20CD);       // INT 20h
    ww(((uint32_t)kPspSeg << 4) + 0x02, alloc_top);    // top of memory
    wb(((uint32_t)kPspSeg << 4) + 0x05, 0x9A);         // far call to DOS
    for (int i = 0x18; i < 0x2C; i++) wb(((uint32_t)kPspSeg << 4) + i, 0xFF);  // handles
    // Optional non-empty command tail, so programs that require being launched
    // from their .BAT wrapper (e.g. LOF's START.BAT check) will proceed.
    if (const char* cl = getenv("LOF_CMDLEN")) {
        int n = atoi(cl);
        if (n < 0) n = 0;
        if (n > 126) n = 126;
        wb(((uint32_t)kPspSeg << 4) + 0x80, (uint8_t)n);
        for (int i = 0; i < n; i++) wb(((uint32_t)kPspSeg << 4) + 0x81 + i, ' ');
        wb(((uint32_t)kPspSeg << 4) + 0x81 + n, 0x0D);
    }
    // MCB for the program block
    uint16_t mcb = kPspSeg - 1;
    uint32_t ma = (uint32_t)mcb << 4;
    wb(ma, 'Z');
    ww(ma + 1, kPspSeg);
    ww(ma + 3, (uint16_t)(alloc_top - kPspSeg + 1));
    // default interrupt vectors to a shared IRET stub (handled in do_int anyway)
    (void)minalloc;
}

// --------------------------------------------------------------- INT 21h ---
struct FileHandle {
    FILE* fp = nullptr;
    bool is_device = false;
    std::string name;
};
static std::map<int, FileHandle> g_handles;
static int g_next_handle = 5;

std::string Machine::resolve(const std::string& dos_path) {
    std::string p = dos_to_host(dos_path);
    // strip a leading "X:" drive letter
    if (p.size() >= 2 && p[1] == ':') p = p.substr(2);
    if (!p.empty() && (p[0] == '/' || p[0] == '\\')) p = p.substr(1);
    if (p.empty()) return game_dir;
    // DOS filenames are case-insensitive; the game stores paths in lower case
    // while the extracted archive uses upper case.  Walk the host tree,
    // matching each component case-insensitively.
    std::string full = game_dir;
    size_t i = 0;
    while (i <= p.size()) {
        size_t j = p.find('/', i);
        std::string comp = p.substr(i, j == std::string::npos ? std::string::npos : j - i);
        if (comp.empty()) { if (j == std::string::npos) break; i = j + 1; continue; }
        std::string lc = comp; for (char& ch : lc) ch = (char)tolower((unsigned char)ch);
        std::string found;
        DIR* d = opendir(full.c_str());
        if (d) {
            struct dirent* e;
            while ((e = readdir(d))) {
                std::string n = e->d_name;
                std::string nl = n; for (char& ch : nl) ch = (char)tolower((unsigned char)ch);
                if (nl == lc) { found = n; break; }
            }
            closedir(d);
        }
        full += "/" + (found.empty() ? comp : found);
        if (j == std::string::npos) break;
        i = j + 1;
    }
    return full;
}

static void set_cf(CPU& c, bool on) { if (on) c.flags |= CF; else c.flags &= ~CF; }

void Machine::dos_int(uint8_t n) {
    if (getenv("LOF_I21ALL")) { static unsigned cnt[256]={0}; static long t=0;
        uint8_t ah=(uint8_t)(cpu.ax>>8); cnt[ah]++;
        if(++t%50000==0) for(int i=0;i<256;i++) if(cnt[i]) fprintf(stderr,"[i21] AH=%02X cnt=%u\n",i,cnt[i]); }
    (void)n;
    CPU& c = cpu;
    uint8_t ah = (uint8_t)(c.ax >> 8);
    uint8_t al = (uint8_t)(c.ax & 0xFF);
    switch (ah) {
        case 0x00: case 0x4C: { exit_code = al;
            if (getenv("LOF_EXITDBG")) fprintf(stderr, "[exit] AH=%02X al=%02X cs:ip=%04X:%04X instr=%lu\n", ah, al, c.cs, (uint16_t)(c.ip-2), (unsigned long)instr_count);
            running = false; break; }
        case 0x01: {   // read char with echo
            if (!key_queue.empty()) { uint8_t k = key_queue.front(); key_queue.erase(key_queue.begin());
                klog("21/01", k);
                c.ax = (uint16_t)((c.ax & 0xFF00) | scancode_ascii(k)); }
            else c.ax = (uint16_t)((c.ax & 0xFF00) | 0);
            break; }
        case 0x02: fputc((char)(c.dx & 0xFF), stdout); fflush(stdout); break;
        case 0x06: {
            if ((c.dx & 0xFF) == 0xFF) {  // direct input, non-blocking
                if (!key_queue.empty()) { uint8_t k = key_queue.front(); key_queue.erase(key_queue.begin());
                    klog("21/06", k);
                    c.ax = (uint16_t)((c.ax & 0xFF00) | scancode_ascii(k)); c.flags &= ~ZF; }
                else { c.ax = (uint16_t)(c.ax & 0xFF00); c.flags |= ZF; }
            } else { fputc((char)(c.dx & 0xFF), stdout); fflush(stdout); }
            break; }
        case 0x07: case 0x08: {
            if (!key_queue.empty()) { uint8_t k = key_queue.front(); key_queue.erase(key_queue.begin());
                klog2("21/07", k, instr_count);
                c.ax = (uint16_t)((c.ax & 0xFF00) | scancode_ascii(k)); }
            else c.ax = (uint16_t)((c.ax & 0xFF00));
            break; }
        case 0x09: {
            uint32_t p = ((uint32_t)c.ds << 4) + c.dx;
            std::string s;
            while (rb(p) != '$') { s += (char)rb(p); p++; }
            if (getenv("LOF_I21")) fprintf(stderr, "[09] %s (ds:dx=%04X:%04X)\n", s.c_str(), c.ds, c.dx);
            fputs(s.c_str(), stdout); fflush(stdout); break; }
        case 0x0A: {  // buffered input
            uint32_t p = ((uint32_t)c.ds << 4) + c.dx;
            uint8_t maxlen = rb(p);
            uint8_t cnt = 0;
            while (!key_queue.empty() && cnt < maxlen) {
                uint8_t ch = key_queue.front(); key_queue.erase(key_queue.begin());
                if (ch == '\r') break;
                wb(p + 2 + cnt, ch); cnt++;
            }
            wb(p + 1, cnt);
            break; }
        case 0x0B: { c.ax = (uint16_t)((c.ax & 0xFF00) | (key_queue.empty() ? 0x00 : 0xFF));
            static int nb=0; if (getenv("LOF_KDBG") && !key_queue.empty() && nb<20){ fprintf(stderr,"[0B] key pending, returns FF\n"); nb++; }
            break; }
        case 0x0C: {
            // DOS 0Ch: flush the type-ahead buffer, then invoke the console
            // input function named in AL (01/06/07/08/0A).  Games poll AH=0Bh
            // first and then call 0Ch to read the key that is waiting, so the
            // read must actually return it.
            uint8_t fn = al;
            if (fn == 0x01 || fn == 0x06 || fn == 0x07 || fn == 0x08) {
                if (!key_queue.empty()) {
                    uint8_t k = key_queue.front(); key_queue.erase(key_queue.begin());
                    klog("21/0C", k);
                    c.ax = (uint16_t)((c.ax & 0xFF00) | scancode_ascii(k));
                } else {
                    c.ax = (uint16_t)((c.ax & 0xFF00) | 0);
                }
            } else {
                if (!key_queue.empty()) key_queue.clear();
            }
            break; }
        case 0x19: c.ax = (uint16_t)((c.ax & 0xFF00) | 2); break;  // current drive C:
        case 0x1A: break;  // set DTA (not tracked: single DTA in PSP:0x80 area used)
        case 0x25: {  // set interrupt vector
            uint32_t p = (uint32_t)(c.ax & 0xFF) * 4;
            if (getenv("LOF_VEC")) fprintf(stderr, "[setvec] INT %02X -> %04X:%04X\n", c.ax & 0xFF, c.ds, c.dx);
            ww(p, c.dx); ww(p + 2, c.ds); break; }
        case 0x2A: {  // get date
            time_t t = time(nullptr); struct tm* tm = localtime(&t);
            c.cx = (uint16_t)(tm->tm_year + 1900);
            c.dx = (uint16_t)(((tm->tm_mon + 1) << 8) | tm->tm_mday);
            c.ax = (uint16_t)((c.ax & 0xFF00) | tm->tm_wday); break; }
        case 0x2C: {  // get time
            // LOF gates the town exit on the wall clock ("the town gates are
            // closed and locked 'till early morning"), so a headless run at the
            // wrong hour cannot leave town.  LOF_HOUR/LOF_MIN override the hour
            // and minute the game sees; LOF_SEC the seconds.
            int hh = 0, mm = 0, ss = 0;
            if (const char* e = getenv("LOF_HOUR")) hh = atoi(e) & 0xFF;
            if (const char* e = getenv("LOF_MIN"))  mm = atoi(e) & 0xFF;
            if (const char* e = getenv("LOF_SEC"))  ss = atoi(e) & 0xFF;
            if (getenv("LOF_HOUR") || getenv("LOF_MIN") || getenv("LOF_SEC")) {
                c.cx = (uint16_t)((hh << 8) | mm);
                c.dx = (uint16_t)((ss << 8) | 0);
            } else {
                time_t t = time(nullptr); struct tm* tm = localtime(&t);
                c.cx = (uint16_t)((tm->tm_hour << 8) | tm->tm_min);
                c.dx = (uint16_t)((tm->tm_sec << 8) | 0);
            }
            break; }
        case 0x2F: c.es = kPspSeg; c.bx = 0x80; break;   // get DTA
        case 0x30: c.ax = 0x0005; c.bx = 0; break;        // DOS 5.0
        case 0x35: {  // get interrupt vector
            uint32_t p = (uint32_t)al * 4;
            c.bx = rw(p); c.es = rw(p + 2); break; }
        case 0x3B: {  // chdir
            uint32_t p = ((uint32_t)c.ds << 4) + c.dx;
            std::string s; while (rb(p)) s += (char)rb(p++);
            set_cf(c, false); break; }
        case 0x3C: {  // create
            uint32_t p = ((uint32_t)c.ds << 4) + c.dx;
            std::string s; while (rb(p)) s += (char)rb(p++);
            std::string host = resolve(s);
            if (getenv("LOF_I21")) fprintf(stderr, "[3C create] '%s' -> %s\n", s.c_str(), host.c_str());
            FILE* fp = fopen(host.c_str(), "wb+");
            if (fp) { g_handles[g_next_handle] = {fp, false, host}; c.ax = (uint16_t)(g_next_handle++); set_cf(c, false); }
            else { c.ax = 3; set_cf(c, true); }
            break; }
        case 0x3D: {  // open
            uint32_t p = ((uint32_t)c.ds << 4) + c.dx;
            std::string s; while (rb(p)) s += (char)rb(p++);
            std::string host = resolve(s);
            if (getenv("LOF_I21")) { char hx[80]; for (int i=0;i<16;i++) sprintf(hx+i*3, "%02X ", rb(p+i)); fprintf(stderr, "[3D open] instr=%u '%s' -> %s cs:ip=%04X:%04X ds=%04X dx=%04X bytes=%s\n", instr_count, s.c_str(), host.c_str(), c.cs, c.ip, c.ds, c.dx, hx); }
            const char* m = (al & 3) == 0 ? "rb" : ((al & 3) == 1 ? "rb+" : "rb+");
            FILE* fp = fopen(host.c_str(), m);
            if (fp) { g_handles[g_next_handle] = {fp, false, host}; c.ax = (uint16_t)(g_next_handle++); set_cf(c, false); }
            else { c.ax = 2; set_cf(c, true); }
            break; }
        case 0x3E: {  // close
            auto it = g_handles.find(al);
            if (it != g_handles.end()) { if (it->second.fp) fclose(it->second.fp); g_handles.erase(it); }
            set_cf(c, false); break; }
        case 0x3F: {  // read
            auto it = g_handles.find((int)c.bx);
            if (it == g_handles.end() || !it->second.fp) { if (getenv("LOF_I21")) fprintf(stderr, "[3F read FAIL] h=%u want=%u cs:ip=%04X:%04X\n", c.bx, c.cx, c.cs, c.ip); c.ax = 5; set_cf(c, true); break; }
            uint32_t p = ((uint32_t)c.ds << 4) + c.dx;
            size_t got = fread(&ram[p], 1, c.cx, it->second.fp);
            if (getenv("LOF_I21")) fprintf(stderr, "[3F read] h=%u want=%u got=%zu file=%s\n", c.bx, c.cx, got, it->second.name.c_str());
            if (getenv("LOF_READ")) fprintf(stderr, "[READ] file=%s want=%u got=%zu dest=%04X:%04X\n", it->second.name.c_str(), c.cx, got, c.ds, c.dx);
            c.ax = (uint16_t)got; set_cf(c, false); break; }
        case 0x40: {  // write
            if (c.bx <= 4) {  // stdout etc.
                uint32_t p = ((uint32_t)c.ds << 4) + c.dx;
                fwrite(&ram[p], 1, c.cx, c.bx == 2 ? stderr : stdout);
                fflush(stdout);
                c.ax = c.cx; set_cf(c, false); break;
            }
            auto it = g_handles.find((int)c.bx);
            if (it == g_handles.end() || !it->second.fp) { c.ax = 5; set_cf(c, true); break; }
            uint32_t p = ((uint32_t)c.ds << 4) + c.dx;
            size_t w = fwrite(&ram[p], 1, c.cx, it->second.fp);
            c.ax = (uint16_t)w; set_cf(c, false); break; }
        case 0x41: {  // delete
            uint32_t p = ((uint32_t)c.ds << 4) + c.dx;
            std::string s; while (rb(p)) s += (char)rb(p++);
            set_cf(c, remove(resolve(s).c_str()) != 0); break; }
        case 0x42: {  // lseek
            auto it = g_handles.find((int)c.bx);
            if (it == g_handles.end() || !it->second.fp) { c.ax = 5; set_cf(c, true); break; }
            uint32_t off = ((uint32_t)c.cx << 16) | c.dx;
            int whence = (al == 0) ? SEEK_SET : (al == 1 ? SEEK_CUR : SEEK_END);
            fseek(it->second.fp, (long)off, whence);
            long pos = ftell(it->second.fp);
            c.dx = (uint16_t)(pos >> 16); c.ax = (uint16_t)(pos & 0xFFFF); set_cf(c, false); break; }
        case 0x43: set_cf(c, false); c.ax = 0; break;   // attributes
        case 0x44: set_cf(c, true); break;              // ioctl (unsupported)
        case 0x47: {  // get current dir
            uint32_t p = ((uint32_t)c.ds << 4) + c.si;
            const char* d = ".";
            while (*d) wb(p++, (uint8_t)*d++);
            wb(p, 0); set_cf(c, false); break; }
        case 0x48: {  // allocate memory
            // Bump allocator above the resident image (set in load_exe) so each
            // program starts with a full, fresh heap; this mirrors DOS handing
            // out the largest free block below the video segment.
            uint16_t want = c.bx;
            if ((uint32_t)alloc_free + want < 0xA000) {
                c.ax = alloc_free; alloc_free += want + 1; set_cf(c, false);
            } else {
                c.ax = (uint16_t)(0xA000 - alloc_free); c.bx = c.ax; set_cf(c, true);
            }
            break; }
        case 0x49: set_cf(c, false); break;             // free memory
        case 0x4A: {  // resize memory block at ES (BX = new size in paragraphs)
            // A DOS .EXE prologue shrinks its own block (ES = PSP) to its real
            // size, then allocates working memory on top.  We must honour the
            // shrink, otherwise AH=48 hands back memory that lies *inside* the
            // program's own stack and the game zeroes its return address.
            uint16_t seg = c.es, newparas = c.bx;
            uint32_t newend = (uint32_t)seg + newparas;
            if (seg == kPspSeg && newend <= alloc_top) {
                alloc_free = (uint16_t)newend;
                ww(((uint32_t)kPspSeg << 4) + 0x02, (uint16_t)newend);
                set_cf(c, false);
            } else {
                // Resizing a sub-block: keep the bump pointer monotonic.
                if (newend > alloc_free && newend <= alloc_top) alloc_free = (uint16_t)newend;
                set_cf(c, false);
            }
            break; }
        case 0x4B: set_cf(c, true); c.ax = 8; break;    // EXEC (unsupported)
        case 0x4E: {  // find first
            uint32_t p = ((uint32_t)c.ds << 4) + c.dx;
            std::string pat; while (rb(p)) pat += (char)rb(p++);
            (void)pat;
            c.ax = 0x12; set_cf(c, true); break; }
        case 0x4F: c.ax = 0x12; set_cf(c, true); break;
        case 0x56: set_cf(c, false); break;             // rename (unsupported)
        case 0x57: set_cf(c, false); break;             // file date
        default: set_cf(c, true); break;
    }
}

// --------------------------------------------------------------- INT 10h ---
static int cur_row = 0, cur_col = 0;

void Machine::bios_int(uint8_t n) {
    if (getenv("LOF_INT10ALL")) { static unsigned cnt[256]={0}; static long t=0;
        uint8_t ah=(uint8_t)(cpu.ax>>8); cnt[ah]++;
        if(++t%20000==0) for(int i=0;i<256;i++) if(cnt[i]) fprintf(stderr,"[i10] AH=%02X cnt=%u\n",i,cnt[i]); }
    (void)n;
    CPU& c = cpu;
    uint8_t ah = (uint8_t)(c.ax >> 8);
    uint8_t al = (uint8_t)(c.ax & 0xFF);
    switch (ah) {
        case 0x00: set_video_mode(c.ax & 0xFF); cur_row = cur_col = 0; break;
        case 0x01: break;   // cursor shape
        case 0x02: cur_row = (c.dx >> 8) & 0xFF; cur_col = c.dx & 0xFF; break;
        case 0x03: c.cx = 0x0607; c.dx = (uint16_t)((cur_row << 8) | cur_col); break;
        case 0x05: break;
        case 0x06: case 0x07: break;  // scroll (leave as-is)
        case 0x08: { uint32_t o = 0xB8000 + (cur_row * 80 + cur_col) * 2;
            c.ax = (uint16_t)(rb(o) | (rb(o + 1) << 8)); break; }
        case 0x09: case 0x0A: {  // write char/attr
            uint32_t o = 0xB8000 + (cur_row * 80 + cur_col) * 2;
            wb(o, (uint8_t)c.ax); wb(o + 1, (uint8_t)c.bx); break; }
        case 0x0C: {  // write graphics pixel
            if (ega.graphics()) {
                int x = c.cx, y = c.dx;
                if (x < 320 && y < 200) {
                    int off = y * 40 + (x >> 3); int bit = 7 - (x & 7);
                    for (int p = 0; p < 4; p++) {
                        uint8_t v = ega.plane[p][off & 0xFFFF];
                        if (al & (1 << p)) v |= (1 << bit); else v &= ~(1 << bit);
                        ega.plane[p][off & 0xFFFF] = v;
                    }
                }
            }
            break; }
        case 0x0D: {  // read graphics pixel
            c.ax = (uint16_t)(c.ax & 0xFF00); break; }
        case 0x0E: {  // teletype output
            uint8_t ch = al;
            if (ch == 0x0D) cur_col = 0;
            else if (ch == 0x0A) cur_row++;
            else if (ch == 0x08) { if (cur_col) cur_col--; }
            else {
                uint32_t o = 0xB8000 + (cur_row * 80 + cur_col) * 2;
                wb(o, ch); wb(o + 1, (uint8_t)(c.bx & 0xFF));
                cur_col++;
                if (cur_col >= 80) { cur_col = 0; cur_row++; }
            }
            break; }
        case 0x0F: c.ax = (uint16_t)((80 << 8) | ega.mode); c.bx = 0; break;
        case 0x10: {  // attribute-controller palette functions
            if (getenv("LOF_INT10P")) fprintf(stderr,"[int10.10] al=%02X bx=%04X cx=%04X dx=%04X es=%04X cs:ip=%04X:%04X\n", al, c.bx, c.cx, c.dx, c.es, c.cs, c.ip);
            if (al == 0x00) {                 // set individual palette register
                ega.attr_pal[c.bx & 0x0F] = (uint8_t)((c.bx >> 8) & 0x3F);
                fb_dirty = true;
            } else if (al == 0x01) {          // overscan (border) color
                ega.attr_overscan = (uint8_t)(c.bx & 0x3F);
            } else if (al == 0x02) {          // set all 16 palette registers from ES:DX
                uint32_t p = ((uint32_t)c.es << 4) + c.dx;
                for (int i = 0; i < 16; i++) ega.attr_pal[i] = rb(p + i) & 0x3F;
                if (getenv("LOF_INT10P")) { fprintf(stderr,"   data:"); for(int i=0;i<16;i++) fprintf(stderr," %02X", rb(p+i)); fprintf(stderr,"\n"); }
                fb_dirty = true;
            } else if (al == 0x03) {          // toggle intensity/blink
                ega.attr_color_select = (ega.attr_color_select & ~0x08) | ((c.bx & 1) << 3);
                fb_dirty = true;
            }
            break; }
        case 0x11: break;             // font select
        case 0x12: {                  // alternate select / EGA info
            if (c.bx >> 8 == 0x10 || (c.bx & 0xFF) == 0x10) {
                // BL=10h: "return EGA information".  A real EGA BIOS reports the
                // adapter here; games probe it to tell EGA from CGA/MDA.
                // BL=03h: 256 KiB EGA.  Games probe this to tell EGA from
                // CGA/MDA; returning 0 makes them fall back to the CGA path.
                c.bx = (uint16_t)((0 << 8) | 3);
                c.cx = 0;
            } else {
                c.ax = (uint16_t)((c.ax & 0xFF00) | 0);  // no alternate select
            }
            break; }
        case 0x13: break;             // write string
        default: break;
    }
}

// --------------------------------------------------------------- INT 16h ---
// Map a Set-1 scan code to its ASCII character (unshifted).  The BIOS returns
// the scan code in AH and the ASCII value in AL; games key off either one.
uint8_t scancode_ascii(uint8_t sc) {
    static const char* low = "1234567890-=";
    static const char* mid = "qwertyuiop[]";
    static const char* high = "asdfghjkl;'`";
    static const char* lowr = "zxcvbnm,./";
    if (sc >= 0x02 && sc <= 0x0D) return (uint8_t)low[sc - 0x02];
    if (sc >= 0x10 && sc <= 0x1B) return (uint8_t)mid[sc - 0x10];
    if (sc >= 0x1E && sc <= 0x28) return (uint8_t)high[sc - 0x1E];
    if (sc >= 0x2C && sc <= 0x35) return (uint8_t)lowr[sc - 0x2C];
    switch (sc) {
        case 0x01: return 0x1B;   // Esc
        case 0x0E: return 0x08;   // Backspace
        case 0x0F: return 0x09;   // Tab
        case 0x1C: return 0x0D;   // Enter
        case 0x1D: return 0x00;   // Ctrl
        case 0x29: return '`';
        case 0x2B: return '\\';   // backslash
        case 0x2A: case 0x36: return 0x00;  // Shift
        case 0x39: return 0x20;   // Space
        default: return 0x00;
    }
}

static void kbd_int(Machine& m, uint8_t) {
    CPU& c = m.cpu;
    uint8_t ah = (uint8_t)(c.ax >> 8);
    switch (ah) {
        case 0x00: case 0x10: {
            if (!m.key_queue.empty()) { uint8_t k = m.key_queue.front(); m.key_queue.erase(m.key_queue.begin());
                klog("int16/00", k);
                if (ah == 0x10) c.ax = (uint16_t)(((c.ax & 0xFF00)) | scancode_ascii(k));
                else c.ax = (uint16_t)((k << 8) | scancode_ascii(k)); }
            else c.ax = (uint16_t)(c.ax & 0xFF00);
            break; }
        case 0x01: case 0x11: {
            if (!m.key_queue.empty()) { c.ax = (uint16_t)((m.key_queue.front() << 8) | scancode_ascii(m.key_queue.front())); c.flags &= ~ZF;
                klog("int16/01peek", m.key_queue.front()); }
            else { c.ax = (uint16_t)(c.ax & 0xFF00); c.flags |= ZF; }
            break; }
        case 0x02: c.ax = (uint16_t)((c.ax & 0xFF00) | m.kbd_shift); break;
        case 0x12: c.ax = (uint16_t)((c.ax & 0xFF00) | m.kbd_shift); break;
        default: break;
    }
}

// --------------------------------------------------------------- INT 33h ---
static void mouse_int(Machine& m, uint8_t) {
    CPU& c = m.cpu;
    if (getenv("LOF_MDBG") && c.ax != 3) fprintf(stderr,"[mousefn] ax=%04X bx=%04X cx=%04X dx=%04X\n", c.ax,c.bx,c.cx,c.dx);
    switch (c.ax) {
        case 0x0000: c.ax = 0xFFFF; c.bx = 2; break;                 // reset
        case 0x0001: m.mouse_visible = true; break;                  // show
        case 0x0002: m.mouse_visible = false; break;                 // hide
        case 0x0003: c.bx = m.mouse_buttons; c.cx = (uint16_t)m.mouse_x; c.dx = (uint16_t)m.mouse_y;
            if (getenv("LOF_MDBG") && m.mouse_buttons) fprintf(stderr,"[mouse3] buttons=%u x=%d y=%d\n", m.mouse_buttons, m.mouse_x, m.mouse_y);
            break;
        case 0x0004: m.mouse_x = c.cx; m.mouse_y = c.dx; break;
        case 0x0005: c.ax = m.mouse_buttons; c.bx = m.mouse_buttons; break;
        case 0x0006: c.ax = m.mouse_buttons; c.bx = m.mouse_buttons; break;
        case 0x0007: m.mouse_minx = c.cx; m.mouse_maxx = c.dx; break;
        case 0x0008: m.mouse_miny = c.cx; m.mouse_maxy = c.dx; break;
        case 0x000B: c.cx = 0; c.dx = 0; break;                      // motion counters
        case 0x000C: break;                                          // set handler (polled)
        default: break;
    }
}

void Machine::do_int(uint8_t n) {
    if (n == 0x21) { count_21++;
        if (getenv("LOF_I21")) {
            static int shown = 0; const char* mx = getenv("LOF_I21MAX"); int lim = mx ? atoi(mx) : 100;
            if (shown < lim) { fprintf(stderr,"INT21 #%d AH=%02X cs:ip=%04X:%04X ax=%04X bx=%04X cx=%04X dx=%04X ds=%04X\n",
                count_21, cpu.ax>>8, cpu.cs, (uint16_t)(cpu.ip-2), cpu.ax, cpu.bx, cpu.cx, cpu.dx, cpu.ds); shown++; }
        } }
    else if (n == 0x10) { count_10++;
        if (getenv("LOF_I10")) { static int shown=0; if (shown<200) { fprintf(stderr,"INT10 AH=%02X AL=%02X bx=%04X cx=%04X dx=%04X cs:ip=%04X:%04X\n", cpu.ax>>8, cpu.ax&0xFF, cpu.bx, cpu.cx, cpu.dx, cpu.cs, (uint16_t)(cpu.ip-2)); shown++; } } }
    else if (n == 0x16) { count_16++; }
    switch (n) {
        case 0x10: bios_int(n); break;
        case 0x16: kbd_int(*this, n); break;
        case 0x21: dos_int(n); break;
        case 0x20: running = false; break;
        case 0x33: mouse_int(*this, n); break;
        case 0x1A: cpu.ax = (uint16_t)((cpu.ax & 0xFF00) | 0); break;   // time (BIOS)
        case 0x11: case 0x12: cpu.ax = 0; break;                        // equipment
        case 0x08: break;                                               // timer IRQ (should not be SW)
        case 0x24: break;                                               // critical error handler
        case 0x2F: cpu.ax = 0; break;                                   // multiplex
        case 0x28: break;                                               // DOS idle
        default: break;
    }
    // Simulate the standard INT frame behaviour: the caller already pushed the
    // frame in cpu.cpp; a plain "iret" returns to the interrupted instruction.
}

void Machine::trigger_int(uint8_t n) {
    // Simulate a hardware interrupt: push FLAGS/CS/IP and jump to the handler
    // stored in the interrupt vector table.
    uint32_t vec = (uint32_t)n * 4;
    uint16_t off = rw(vec), seg = rw(vec + 2);
    if (off == 0 && seg == 0) return;
    auto push = [&](uint16_t v) { cpu.sp = (uint16_t)(cpu.sp - 2); ww(((uint32_t)cpu.ss << 4) + cpu.sp, v); };
    push(cpu.flags);
    push(cpu.cs);
    push(cpu.ip);
    cpu.flags &= ~(IF | TF);
    cpu.cs = seg;
    cpu.ip = off;
}

// ---------------------------------------------------------------- run ------
void Machine::run() {
    cpu.m = this;
    while (running) {
        for (int i = 0; i < 20000 && running; i++) {
            cpu_step(cpu);
            instr_count++;
            if (cpu.halted) { cpu.halted = false; break; }
        }
        ticks++;
        ww(0x46C, (uint16_t)(ticks & 0xFFFF));
        if (!getenv("LOF_NO_TIMER") && (cpu.flags & IF)) trigger_int(0x1C);
        if (pump) pump();
    }
}

}  // namespace lof
