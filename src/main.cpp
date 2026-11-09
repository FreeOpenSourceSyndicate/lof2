// Front-end: loads VOR.EXE/LOF.EXE into the emulator, drives it from an SDL2
// window (or headless), and exposes the game directory as the DOS C: drive.
//
// Usage:
//   lof-native [--dir DIR] [--headless] [--frames N] [--shot FILE.png]
//              [--keys "scancode,..."] [--timeout SEC]
//
// The two original programs are run in sequence exactly like START.BAT does:
// VOR (copy protection) first, then LOF (the game).
#include "emu.h"

#include <algorithm>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <string>
#include <sstream>

#include <vector>

#ifdef USE_SDL
#include <SDL.h>
#endif

using namespace lof;

static std::vector<uint8_t> read_file(const std::string& path, bool* ok = nullptr) {
    std::vector<uint8_t> v;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) { if (ok) *ok = false; return v; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    v.resize(n > 0 ? n : 0);
    if (n > 0) fread(v.data(), 1, n, f);
    fclose(f);
    if (ok) *ok = true;
    return v;
}

static void write_png(const std::string& path, const std::vector<uint32_t>& fb, int w, int h);

struct Options {
    std::string dir = ".";
    bool dir_set = false;
    bool headless = false;
    long frames = -1;
    std::string shot;
    long shot_frame = 0;
    std::vector<uint8_t> keys;
    double timeout = -1;
    bool run_vor = false;   // VOR is disk copy-protection only; see below
    std::string exe;   // run a single EXE instead of LOF
};

static Options parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : std::string(); };
        if (a == "--dir") { o.dir = next(); o.dir_set = true; }
        else if (a == "--headless") o.headless = true;
        else if (a == "--frames") o.frames = atol(next().c_str());
        else if (a == "--shot") o.shot = next();
        else if (a == "--shot-frame") o.shot_frame = atol(next().c_str());
        else if (a == "--timeout") o.timeout = atof(next().c_str());
        else if (a == "--vor") o.run_vor = true;   // opt in to the VOR preloader
        else if (a == "--exe") { o.exe = next(); o.run_vor = false; }
        else if (a == "--keys") {
            std::string s = next(); size_t p = 0;
            while (p < s.size()) { size_t c = s.find(',', p);
                std::string t = s.substr(p, c == std::string::npos ? std::string::npos : c - p);
                if (!t.empty()) o.keys.push_back((uint8_t)strtol(t.c_str(), nullptr, 0));
                if (c == std::string::npos) break;
                p = c + 1; }
        }
    }
    return o;
}

int main(int argc, char** argv) {
    Options opt = parse(argc, argv);
    // Drop-in mode: when invoked as "vor" (the name START.BAT uses for the
    // copy-protection preloader), reproduce the original startup sequence.
    {
        std::string a0 = argv[0] ? argv[0] : "";
        size_t s = a0.find_last_of('/');
        std::string base = (s == std::string::npos) ? a0 : a0.substr(s + 1);
        if (base == "vor" || base == "vor.exe" || base == "VOR.EXE") opt.run_vor = true;
    }
    if (!opt.dir_set) {
        // Drop-in mode: run from the directory holding this executable so the
        // binary can sit next to LOF.EXE/VOR.EXE inside the game folder.
        char buf[4096];
        ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
        if (n > 0) {
            buf[n] = '\0';
            std::string p(buf);
            size_t slash = p.find_last_of('/');
            if (slash != std::string::npos) opt.dir = p.substr(0, slash);
        }
    }

    Machine m;
    m.game_dir = opt.dir;
    m.reset_cpu_state();

    std::string exe = opt.exe.empty() ? (opt.dir + "/LOF.EXE") : opt.exe;
    std::string vor = opt.dir + "/VOR.EXE";

#ifdef USE_SDL
    SDL_Window* win = nullptr;
    SDL_Renderer* ren = nullptr;
    SDL_Texture* tex = nullptr;
    bool sdl_ok = false;
    if (!opt.headless) {
        if (SDL_Init(SDL_INIT_VIDEO) == 0) {
            win = SDL_CreateWindow("Legend of Faerghail", SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED, 640, 400, SDL_WINDOW_SHOWN);
            if (win) {
                ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
                if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
                if (ren) { tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
                    SDL_TEXTUREACCESS_STREAMING, 640, 400); sdl_ok = tex != nullptr; }
            }
        }
        if (!sdl_ok) { opt.headless = true; }
    }
#else
    opt.headless = true;
#endif

    long frame_no = 0;
    auto present = [&]() {
        m.render();
        if (!opt.shot.empty() && frame_no == opt.shot_frame) {
            write_png(opt.shot, m.framebuffer, m.fb_w, m.fb_h); fprintf(stderr,"[lof] shot fb=%dx%d\n",m.fb_w,m.fb_h);
        }
#ifdef USE_SDL
        if (sdl_ok) {
            // stretch to 640x400 with correct aspect
            void* pixels = nullptr; int pitch = 0;
            static std::vector<uint32_t> scaled;
            scaled.assign(640 * 400, 0xFF000000u);
            int sx = 640 / m.fb_w, sy = 400 / m.fb_h;
            if (sx < 1) sx = 1;
            if (sy < 1) sy = 1;
            for (int y = 0; y < 400; y++)
                for (int x = 0; x < 640; x++) {
                    int gx = x / sx, gy = y / sy;
                    if (gx < m.fb_w && gy < m.fb_h)
                        scaled[y * 640 + x] = m.framebuffer[gy * m.fb_w + gx];
                }
            SDL_UpdateTexture(tex, nullptr, scaled.data(), 640 * 4);
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex, nullptr, nullptr);
            SDL_RenderPresent(ren);
            (void)pixels; (void)pitch;
        }
#endif
        frame_no++;
    };

    long key_frame = getenv("LOF_KEYFRAME") ? atol(getenv("LOF_KEYFRAME")) : 5;
    // LOF_KEYONCE=1 injects the --keys sequence exactly once (at key_frame)
    // instead of re-queueing it every 20 frames.  The repeating default makes
    // scripted menus work, but it re-presses every hot-key on a 20-frame cycle,
    // which loops multi-step dialogs (e.g. town -> Continue) back to the start.
    bool key_once = getenv("LOF_KEYONCE") && atoi(getenv("LOF_KEYONCE")) != 0;
    // LOF_KEYSTEP=N delivers one --keys entry every N frames (starting at
    // LOF_KEYFRAME) instead of dumping the whole list at once.  Multi-screen
    // flows (menu -> open file -> town -> Continue) need each key to arrive
    // after the previous screen has actually drawn and started polling.
    long key_step = getenv("LOF_KEYSTEP") ? atol(getenv("LOF_KEYSTEP")) : 0;
    // LOF_KEYAT="frame:sc,frame:sc,..." queues each scancode at an exact frame.
    // Multi-screen flows have wildly different dwell times (loading a save runs
    // tens of millions of instructions), so fixed-step delivery overshoots;
    // this lets a script pin each key to the screen that is actually waiting.
    struct KeyAt { long frame; uint8_t sc; };
    std::vector<KeyAt> key_at;
    if (const char* ka = getenv("LOF_KEYAT")) {
        std::string s(ka), item;
        std::stringstream ss(s);
        while (std::getline(ss, item, ',')) {
            auto c = item.find(':');
            if (c == std::string::npos) continue;
            long fr = strtol(item.substr(0, c).c_str(), nullptr, 0);
            long sc = strtol(item.substr(c + 1).c_str(), nullptr, 0);
            key_at.push_back({fr, (uint8_t)sc});
        }
    }
    size_t key_idx = 0;
    bool key_sent = false;
    // LOF_KEYAUTO="sc,sc,...": deliver one scancode at a time, but only once the
    // game has drained the previous one from key_queue.  Screen dwell times vary
    // wildly (loading the town or a map runs tens of millions of instructions),
    // so frame-scheduled delivery drops keys; gating on an empty queue adapts.
    std::vector<uint8_t> key_auto;
    if (const char* ka = getenv("LOF_KEYAUTO")) {
        std::string s(ka), item; std::stringstream ss(s);
        while (std::getline(ss, item, ',')) if (!item.empty())
            key_auto.push_back((uint8_t)strtol(item.c_str(), nullptr, 0));
    }
    size_t auto_idx = 0;
    long key_gap = getenv("LOF_KEYGAP") ? atol(getenv("LOF_KEYGAP")) : 30;
    long next_auto_frame = key_gap;
    auto feed_keys = [&]() {
        if (!key_auto.empty()) {
            if (auto_idx < key_auto.size() && m.key_queue.empty() && frame_no >= next_auto_frame) {
                m.key_queue.push_back(key_auto[auto_idx++]);
                if (getenv("LOF_KEYLOG")) fprintf(stderr, "[keylog] pushed sc=%02X frame=%ld\n", m.key_queue.back(), frame_no);
                next_auto_frame = frame_no + key_gap;
            }
        } else if (!key_at.empty()) {
            while (key_idx < key_at.size() && frame_no >= key_at[key_idx].frame) {
                m.key_queue.push_back(key_at[key_idx].sc);
                key_idx++;
            }
        } else if (!opt.keys.empty() && frame_no >= key_frame) {
            if (key_step > 0) {
                size_t want = (size_t)((frame_no - key_frame) / key_step) + 1;
                while (key_idx < opt.keys.size() && key_idx < want) {
                    m.key_queue.push_back(opt.keys[key_idx++]);
                }
            } else if (key_once ? !key_sent : ((frame_no - key_frame) % 20) == 0) {
                for (uint8_t k : opt.keys) m.key_queue.push_back(k);
                key_sent = true;
            }
        }
        if (getenv("LOF_MOVEX")) m.mouse_x = atoi(getenv("LOF_MOVEX"));
        if (getenv("LOF_MOVEY")) m.mouse_y = atoi(getenv("LOF_MOVEY"));
        if (getenv("LOF_CLICK")) {
            // Click at (mouse_x, mouse_y) starting at frame LOF_CLICKFRAME, held
            // for a few frames so the game's polling loop sees the button down.
            // Repeat every LOF_CLICKPER frames (default 40) if LOF_CLICKREP set.
            long cf = getenv("LOF_CLICKFRAME") ? atol(getenv("LOF_CLICKFRAME")) : 60;
            long per = getenv("LOF_CLICKPER") ? atol(getenv("LOF_CLICKPER")) : 40;
            long rep = getenv("LOF_CLICKREP") ? atol(getenv("LOF_CLICKREP")) : 0;
            long n = frame_no - cf;
            if (n >= 0 && (rep == 0 ? (n < 6) : (n % per) < 6)) m.mouse_buttons = 1;
            else m.mouse_buttons = 0;
        }
    };

#ifdef USE_SDL
    auto handle_events = [&]() {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { m.running = false; }
            else if (e.type == SDL_KEYDOWN) {
                SDL_Keycode k = e.key.keysym.sym;
                uint8_t sc = 0;
                switch (k) {
                    case SDLK_ESCAPE: sc = 0x01; break;
                    case SDLK_RETURN: sc = 0x1C; break;
                    case SDLK_SPACE: sc = 0x39; break;
                    case SDLK_UP: sc = 0x48; break;
                    case SDLK_DOWN: sc = 0x50; break;
                    case SDLK_LEFT: sc = 0x4B; break;
                    case SDLK_RIGHT: sc = 0x4D; break;
                    case SDLK_a: sc = 0x1E; break;
                    case SDLK_b: sc = 0x30; break;
                    case SDLK_c: sc = 0x2E; break;
                    case SDLK_d: sc = 0x20; break;
                    case SDLK_e: sc = 0x12; break;
                    case SDLK_f: sc = 0x21; break;
                    case SDLK_g: sc = 0x22; break;
                    case SDLK_h: sc = 0x23; break;
                    case SDLK_i: sc = 0x17; break;
                    case SDLK_j: sc = 0x24; break;
                    case SDLK_k: sc = 0x25; break;
                    case SDLK_l: sc = 0x26; break;
                    case SDLK_m: sc = 0x32; break;
                    case SDLK_n: sc = 0x31; break;
                    case SDLK_o: sc = 0x18; break;
                    case SDLK_p: sc = 0x19; break;
                    case SDLK_q: sc = 0x10; break;
                    case SDLK_r: sc = 0x13; break;
                    case SDLK_s: sc = 0x1F; break;
                    case SDLK_t: sc = 0x14; break;
                    case SDLK_u: sc = 0x16; break;
                    case SDLK_v: sc = 0x2F; break;
                    case SDLK_w: sc = 0x11; break;
                    case SDLK_x: sc = 0x2D; break;
                    case SDLK_y: sc = 0x15; break;
                    case SDLK_z: sc = 0x2C; break;
                    case SDLK_1: sc = 0x02; break; case SDLK_2: sc = 0x03; break;
                    case SDLK_3: sc = 0x04; break; case SDLK_4: sc = 0x05; break;
                    case SDLK_5: sc = 0x06; break; case SDLK_6: sc = 0x07; break;
                    case SDLK_7: sc = 0x08; break; case SDLK_8: sc = 0x09; break;
                    case SDLK_9: sc = 0x0A; break; case SDLK_0: sc = 0x0B; break;
                    default: sc = 0; break;
                }
                if (sc) m.key_queue.push_back(sc);
            }
        }
    };
#endif

    long trace_from = getenv("LOF_TRACE_FROM") ? atol(getenv("LOF_TRACE_FROM")) : -1;
    long trace_left = getenv("LOF_TRACE") ? atol(getenv("LOF_TRACE")) : 0;
    auto run_one = [&](const std::string& path) -> bool {
        bool ok = false;
        std::vector<uint8_t> img = read_file(path, &ok);
        if (!ok) { fprintf(stderr, "cannot open %s\n", path.c_str()); return false; }
        m.running = true;
        m.reset_cpu_state();
        m.load_exe(img, path);
        if (getenv("LOF_DISASM")) {
            // LOF_DISASM=seg:off:len — hex-dump the loaded image at that address.
            unsigned seg=0, off=0, len=32; sscanf(getenv("LOF_DISASM"), "%x:%x:%u", &seg, &off, &len);
            uint32_t base = ((uint32_t)seg << 4) + off;
            fprintf(stderr, "[disasm] %04X:%04X =", seg, off);
            for (unsigned i = 0; i < len; i++) fprintf(stderr, " %02X", m.rb(base + i));
            fprintf(stderr, "\n");
        }
        // Run in chunks, presenting a frame after each chunk.
        std::vector<std::string> ring;
        ring.reserve(64);
        while (m.running) {
            for (int i = 0; i < 300000 && m.running; i++) {
                uint32_t pc0 = (uint32_t)m.cpu.cs * 16u + m.cpu.ip;
                if (trace_left > 0 && (trace_from < 0 || (long)m.instr_count >= trace_from)) {
                    fprintf(stderr, "%08u %04X:%04X %02X%02X%02X%02X%02X%02X | ax=%04X bx=%04X cx=%04X dx=%04X si=%04X di=%04X bp=%04X sp=%04X ds=%04X es=%04X ss=%04X fl=%04X\n",
                        m.instr_count, m.cpu.cs, m.cpu.ip,
                        m.rb(pc0), m.rb(pc0 + 1), m.rb(pc0 + 2), m.rb(pc0 + 3), m.rb(pc0 + 4), m.rb(pc0 + 5),
                        m.cpu.ax, m.cpu.bx, m.cpu.cx, m.cpu.dx,
                        m.cpu.si, m.cpu.di, m.cpu.bp, m.cpu.sp, m.cpu.ds, m.cpu.es, m.cpu.ss, m.cpu.flags);
                    trace_left--;
                }
                if (const char* tip = getenv("LOF_TRACE_IP")) {
                    static long match = -1, left = 0;
                    if (match < 0) { match = strtol(tip, nullptr, 16); char* l = getenv("LOF_TRACE_N"); left = l ? atol(l) : 200; }
                    if ((long)(m.cpu.cs * 16u + m.cpu.ip) == match && left > 0) {
                        fprintf(stderr, "T %08u %04X:%04X | ax=%04X bx=%04X cx=%04X dx=%04X si=%04X di=%04X bp=%04X sp=%04X ds=%04X es=%04X ss=%04X fl=%04X\n",
                            m.instr_count, m.cpu.cs, m.cpu.ip, m.cpu.ax, m.cpu.bx, m.cpu.cx, m.cpu.dx,
                            m.cpu.si, m.cpu.di, m.cpu.bp, m.cpu.sp, m.cpu.ds, m.cpu.es, m.cpu.ss, m.cpu.flags);
                        left--;
                    }
                }
                if (getenv("LOF_RING")) {
                    char buf[160];
                    snprintf(buf, sizeof buf, "%08u %04X:%04X %02X | ax=%04X bx=%04X cx=%04X dx=%04X si=%04X di=%04X bp=%04X sp=%04X ds=%04X es=%04X ss=%04X fl=%04X",
                        m.instr_count, m.cpu.cs, m.cpu.ip, m.rb(pc0), m.cpu.ax, m.cpu.bx, m.cpu.cx, m.cpu.dx,
                        m.cpu.si, m.cpu.di, m.cpu.bp, m.cpu.sp, m.cpu.ds, m.cpu.es, m.cpu.ss, m.cpu.flags);
                    if (ring.size() < 256) ring.push_back(buf);
                    else { ring[m.instr_count % 256] = buf; }
                }
                cpu_step(m.cpu);
                if (getenv("LOF_CSCHG")) {
                    static uint16_t prev_cs = 0xFFFF;
                    static uint16_t prev_ss = 0xFFFF;
                    if (m.cpu.cs != prev_cs) {
                        fprintf(stderr, "CSCHG %08u -> %04X:%04X (from %04X:%04X) ax=%04X bx=%04X cx=%04X dx=%04X si=%04X di=%04X bp=%04X sp=%04X\n",
                            m.instr_count, m.cpu.cs, m.cpu.ip, prev_cs, m.cpu.ip, m.cpu.ax, m.cpu.bx, m.cpu.cx,
                            m.cpu.dx, m.cpu.si, m.cpu.di, m.cpu.bp, m.cpu.sp);
                        prev_cs = m.cpu.cs;
                    }
                    if (m.cpu.ss != prev_ss) {
                        fprintf(stderr, "SSCHG %08u ss=%04X sp=%04X cs:ip=%04X:%04X ax=%04X bx=%04X cx=%04X dx=%04X si=%04X di=%04X bp=%04X\n",
                            m.instr_count, m.cpu.ss, m.cpu.sp, m.cpu.cs, m.cpu.ip, m.cpu.ax, m.cpu.bx,
                            m.cpu.cx, m.cpu.dx, m.cpu.si, m.cpu.di, m.cpu.bp);
                        prev_ss = m.cpu.ss;
                    }
                }
                if (m.cpu.cs == 0) {
                    if (getenv("LOF_CRASHDBG")) {
                        fprintf(stderr, "[crash] cs:ip=%04X:%04X ss:sp=%04X:%04X instr=%u\n",
                                m.cpu.cs, m.cpu.ip, m.cpu.ss, m.cpu.sp, m.instr_count);
                        uint32_t sbase = (uint32_t)m.cpu.ss << 4;
                        for (int k = -8; k <= 16; k++) {
                            uint32_t a = sbase + (uint16_t)(m.cpu.sp + k);
                            fprintf(stderr, "  ss:[sp%+d]=%04X\n", k, (uint16_t)(m.rb(a) | (m.rb(a + 1) << 8)));
                        }
                    }
                    m.running = false;
                }
                if (!m.running) {
                    for (auto& s : ring) fprintf(stderr, "%s\n", s.c_str());
                    break;
                }
                m.instr_count++;
                if (m.cpu.halted) { m.cpu.halted = false; break; }
            }
            if (getenv("LOF_FTRACE")) {
                size_t nz=0; for (int pp=0;pp<4;pp++) for (uint8_t b : m.ega.plane[pp]) if(b) nz++;
                fprintf(stderr,"[ft] frame=%ld mode=%d nz=%zu egaw=%llu cs:ip=%04X:%04X\n", frame_no, m.ega.mode, nz, (unsigned long long)m.ega.writes, m.cpu.cs, m.cpu.ip);
            }
            m.ticks++;
            m.ww(0x46C, (uint16_t)(m.ticks & 0xFFFF));
            feed_keys();
#ifdef USE_SDL
            handle_events();
#endif
            present();
            if (opt.frames >= 0 && frame_no >= opt.frames) { m.running = false; }
        }
        return true;
    };

    // START.BAT runs VOR before LOF, but VOR is purely disk copy-protection
    // (it validates the disk id and aborts with "LEGEND OF FAERGHAIL aborted,
    // good bye..." on failure); it draws no game content.  We therefore skip
    // it by default so the game can run from a plain folder.  Pass --vor to
    // reproduce the original startup sequence.
    if (opt.run_vor) {
        run_one(vor);
        frame_no = 0;
    }
    run_one(exe);

    if (getenv("LOF_DEBUG")) {
        size_t nz = 0;
        for (int p = 0; p < 4; p++)
            for (uint8_t b : m.ega.plane[p]) if (b) nz++;
        fprintf(stderr, "[lof] instr=%u frames=%ld running=%d cs:ip=%04X:%04X mode=%d nzplane=%zu int21=%u int10=%u int16=%u\n",
                m.instr_count, frame_no, (int)m.running, m.cpu.cs, m.cpu.ip, m.ega.mode, nz,
                m.count_21, m.count_10, m.count_16);
        if (getenv("LOF_VDBG")) {
            size_t nonblack = 0;
            for (uint32_t p : m.framebuffer) if ((p & 0xFFFFFF) != 0) nonblack++;
            fprintf(stderr, "[vdbg] fb_nonblack=%zu pal:", nonblack);
            if (getenv("LOF_PAL2")) { fprintf(stderr,"\n"); for(int i=0;i<16;i++) fprintf(stderr,"  pal[%d]=%02X\n", i, m.ega.attr_pal[i]); }
            for (int i = 0; i < 16; i++) fprintf(stderr, " %02X", m.ega.attr_pal[i]);
            fprintf(stderr, "\n[vdbg] seq_mask=%02X gc_mode=%02X gc_rot=%02X bpp=%d\n",
                    m.ega.seq_map_mask, m.ega.gc_mode, m.ega.gc_rot, m.ega.bpp());
            fprintf(stderr, "[vdbg] egawrites=%llu\n", (unsigned long long)m.ega.writes);
        }
    }

      if (!opt.shot.empty() && frame_no > 0) {
          write_png(opt.shot, m.framebuffer, m.fb_w, m.fb_h);
          fprintf(stderr,"[lof] shot fb=%dx%d\n",m.fb_w,m.fb_h);
      }

      if (const char* ep = getenv("LOF_EDUMP")) {
          FILE* f = fopen(ep, "wb");
          if (f) { for (int p = 0; p < 4; p++) fwrite(m.ega.plane[p].data(), 1, 0x10000, f); fclose(f);
                   fprintf(stderr, "[lof] dumped 4 EGA planes to %s\n", ep); }
      }
      if (const char* dp = getenv("LOF_DUMP")) {
          FILE* f = fopen(dp, "wb");
          if (f) { fwrite(m.mem(), 1, 1u << 20, f); fclose(f); fprintf(stderr, "[lof] dumped 1MB to %s\n", dp); }
      }

#ifdef USE_SDL
    if (sdl_ok) { SDL_DestroyTexture(tex); SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit(); }
#endif
    return 0;
}

// ---------------------------------------------------------------- PNG out ---
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
static uint32_t crc32(const uint8_t* d, size_t n, uint32_t crc = 0xFFFFFFFFu) {
    static uint32_t tab[256]; static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            tab[i] = c;
        }
        init = true;
    }
    for (size_t i = 0; i < n; i++) crc = tab[(crc ^ d[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}
static void be32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back((uint8_t)(x >> 24)); v.push_back((uint8_t)(x >> 16));
    v.push_back((uint8_t)(x >> 8)); v.push_back((uint8_t)x);
}
static void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
    be32(out, (uint32_t)data.size());
    std::vector<uint8_t> t(type, type + 4);
    std::vector<uint8_t> both = t; both.insert(both.end(), data.begin(), data.end());
    out.insert(out.end(), t.begin(), t.end());
    out.insert(out.end(), data.begin(), data.end());
    be32(out, crc32(both.data(), both.size()));
}
// Minimal zlib stream with stored (uncompressed) deflate blocks.  Avoids a
// libz dependency; PNG readers accept it as a valid IDAT payload.
static std::vector<uint8_t> zlib_stored(const std::vector<uint8_t>& raw) {
    std::vector<uint8_t> z = {0x78, 0x01};
    size_t i = 0;
    do {
        size_t n = std::min<size_t>(65535, raw.size() - i);
        bool last = (i + n >= raw.size());
        z.push_back(last ? 1 : 0);
        z.push_back((uint8_t)(n & 0xFF)); z.push_back((uint8_t)(n >> 8));
        z.push_back((uint8_t)(~n & 0xFF)); z.push_back((uint8_t)((~n >> 8) & 0xFF));
        z.insert(z.end(), raw.begin() + i, raw.begin() + i + n);
        i += n;
    } while (i < raw.size());
    uint32_t a = 1, b = 0;
    for (uint8_t c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
    uint32_t adler = (b << 16) | a;
    be32(z, adler);
    return z;
}
static void write_png_impl(const std::string& path, const std::vector<uint32_t>& fb, int w, int h) {
    std::vector<uint8_t> raw;
    for (int y = 0; y < h; y++) {
        raw.push_back(0);
        for (int x = 0; x < w; x++) {
            uint32_t p = fb[(size_t)y * w + x];
            raw.push_back((p >> 16) & 0xFF); raw.push_back((p >> 8) & 0xFF);
            raw.push_back(p & 0xFF); raw.push_back((p >> 24) & 0xFF);
        }
    }
    std::vector<uint8_t> z = zlib_stored(raw);
    std::vector<uint8_t> out = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<uint8_t> ihdr; be32(ihdr, w); be32(ihdr, h);
    ihdr.push_back(8); ihdr.push_back(6); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
    chunk(out, "IHDR", ihdr);
    chunk(out, "IDAT", z);
    chunk(out, "IEND", {});
    FILE* f = fopen(path.c_str(), "wb");
    if (f) { fwrite(out.data(), 1, out.size(), f); fclose(f); }
}
}  // namespace

static void write_png(const std::string& path, const std::vector<uint32_t>& fb, int w, int h) {
    write_png_impl(path, fb, w, h);
}
