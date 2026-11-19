// Self-test for the EGA image decoder (port/gfx.cpp::load_ega_image).
//
// On-disk layout (verified against every EGA/ file): byte 0 is the RLE escape,
// bytes 1..10 are the ten header bytes (0x1234 magic, width_bytes, height,
// size+10, two reserved), and the RLE stream starts at byte 11.
//
// The engine consumes that stream two ways, and the header's size field
// (= 4*w*h + 10) shows the ten header bytes are part of the decoded buffer:
//
//  * Images: the loader (image 0x1C70) leaves si on the 0x1234 magic and the
//    blitter reads the pattern at [si+0xa].  So the decoder must decode from
//    file byte 1 and drop the ten decoded header bytes.  Decoding from byte 11
//    instead truncates any run that straddles the header boundary (CH25 loses
//    252 bytes, leaving an under-filled sprite).
//  * Fonts (CHR1/CHR2, 40x24): read at file offset 11 directly.
//
// The test builds synthetic files in that layout, so it needs no game data.
#include "gfx.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using namespace lofport;

static std::vector<uint8_t> make_file(uint8_t esc, int wb, int h,
                                      const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> f;
    f.push_back(esc);
    f.push_back(0x34); f.push_back(0x12);                 // magic at bytes 1,2
    f.push_back((uint8_t)(wb & 0xff)); f.push_back((uint8_t)(wb >> 8));
    f.push_back((uint8_t)(h & 0xff));  f.push_back((uint8_t)(h >> 8));
    uint32_t sz = (uint32_t)(4 * wb * h + 10);
    f.push_back((uint8_t)(sz & 0xff)); f.push_back((uint8_t)(sz >> 8));
    f.push_back(0); f.push_back(0);                       // two reserved
    for (uint8_t v : payload) {                           // RLE from byte 11
        if (v == esc) { f.push_back(esc); f.push_back(1); f.push_back(esc); }
        else f.push_back(v);
    }
    return f;
}

static void write_file(const std::string& path, const std::vector<uint8_t>& b) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(b.data()), (std::streamsize)b.size());
}

// An image (not a 40x24 font): the decoder must skip the ten header bytes and
// return exactly the payload, including a run that straddles the boundary.
static void test_image_path(const std::string& dir) {
    const int wb = 3, h = 2;                              // 24-byte payload
    std::vector<uint8_t> payload((size_t)(4 * wb * h));
    for (size_t i = 0; i < payload.size(); i++) payload[i] = (uint8_t)(0x30 + i);
    auto f = make_file(0xFE, wb, h, payload);
    std::string p = dir + "/ega_image_test.bin";
    write_file(p, f);
    EgaImage img;
    std::string err;
    assert(load_ega_image(p, img, &err));
    assert(img.width_bytes == wb && img.height == h);
    assert(img.planes.size() == payload.size());
    for (size_t i = 0; i < payload.size(); i++)
        assert(img.planes[i] == payload[i]);
    std::remove(p.c_str());
}

// A 40x24 font strip: the decoder reads the RLE stream from byte 11, so the ten
// header bytes are ignored and the payload comes back verbatim.
static void test_font_path(const std::string& dir) {
    const int wb = 40, h = 24;
    std::vector<uint8_t> payload((size_t)(4 * wb * h));
    for (size_t i = 0; i < payload.size(); i++) payload[i] = (uint8_t)(i & 0x7f);
    auto f = make_file(0xFE, wb, h, payload);
    std::string p = dir + "/ega_font_test.bin";
    write_file(p, f);
    EgaImage img;
    std::string err;
    assert(load_ega_image(p, img, &err));
    assert(img.planes.size() == payload.size());
    for (size_t i = 0; i < payload.size(); i++)
        assert(img.planes[i] == payload[i]);
    std::remove(p.c_str());
}

// A run that crosses the header boundary must not be lost.  Real files place
// the escape byte inside the header's two reserved bytes (CH25 has `e5 ff 00`
// at file bytes 9..11), so the run it introduces straddles the ten-byte skip.
// Decoding from byte 11 drops that whole run; decoding from byte 1 keeps it.
static void test_boundary_run(const std::string& dir) {
    const int wb = 2, h = 1;                              // 8-byte payload
    const uint8_t esc = 0xE5;
    std::vector<uint8_t> f;
    f.push_back(esc);                                     // byte 0
    f.push_back(0x34); f.push_back(0x12);                 // magic
    f.push_back(wb); f.push_back(0);                      // width_bytes
    f.push_back(h);  f.push_back(0);                      // height
    uint32_t sz = (uint32_t)(4 * wb * h + 10);
    f.push_back((uint8_t)(sz & 0xff)); f.push_back((uint8_t)(sz >> 8));
    f.push_back(esc);                                     // reserved byte 1 = escape
    f.push_back(12);                                      // run count
    f.push_back(0x77);                                    // run value
    std::string p = dir + "/ega_boundary_test.bin";
    write_file(p, f);
    EgaImage img;
    std::string err;
    assert(load_ega_image(p, img, &err));
    assert(img.width_bytes == wb && img.height == h);
    assert(img.planes.size() == (size_t)(4 * wb * h));
    for (size_t i = 0; i < img.planes.size(); i++)
        assert(img.planes[i] == 0x77);                    // run survived the skip
    std::remove(p.c_str());
}

int main() {
    const std::string dir = "/tmp";
    test_image_path(dir);
    test_font_path(dir);
    test_boundary_run(dir);
    std::printf("ega_decode_test: ok\n");
    return 0;
}
