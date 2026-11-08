// Self-test for the character / item / scenario data readers (workstream B).
//
// The synthetic part always runs (no game data required) and checks the SPEC
// record layout and field decoding.  When the game data is present it also
// checks the real SYST/SPEC0 and ROST/ROST files against known values.
#include "char.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace lofport;

static void put16(std::vector<uint8_t>& v, size_t off, uint16_t val) {
    v[off] = (uint8_t)(val & 0xff);
    v[off + 1] = (uint8_t)(val >> 8);
}

static void putstr(std::vector<uint8_t>& v, size_t off, const char* s, size_t limit) {
    size_t n = std::strlen(s);
    assert(n < limit);
    std::memcpy(&v[off], s, n);
    v[off + n] = 0;
}

// Build a one-record SPEC image with recognisable field values and confirm the
// parser recovers every field.
static void synthetic_spec() {
    std::vector<uint8_t> img(1 + kSpecRecordSize, 0);
    img[0] = 1;
    uint8_t* r = &img[1];

    // text_a / text_b
    putstr(img, 1 + 0x0e, "Hello world", 0x4a - 0x0e);
    putstr(img, 1 + 0x50, "A big pile of bones.", 0xc6 - 0x50);

    r[0x4b] = 1;
    put16(img, 1 + 0x4c, 0x1234);
    put16(img, 1 + 0x4e, (uint16_t)(int16_t)-4);
    r[0xc9] = 7;
    put16(img, 1 + 0xda, 321);
    put16(img, 1 + 0xe2, 25);   // SPEC_TYPE_ELEMENTAL
    put16(img, 1 + 0xe4, 9);
    put16(img, 1 + 0xe6, 111);
    put16(img, 1 + 0xe8, 222);

    SpecTable t;
    assert(parse_spec(img, t));
    assert(t.ok);
    assert(t.declared_count == 1);
    assert(t.records.size() == 1);

    const SpecRecord& rec = t.records[0];
    assert(rec.text_a == "Hello world");
    assert(rec.text_b == "A big pile of bones.");
    assert(rec.flag_4b == 1);
    assert(rec.arg_4c == 0x1234);
    assert(rec.arg_4e == -4);
    assert(rec.index_c9 == 7);
    assert(rec.has_index());
    assert(rec.arg_da == 321);
    assert(rec.type == SPEC_TYPE_ELEMENTAL);
    assert(rec.counter == 9);
    assert(rec.arg_e6 == 111);
    assert(rec.arg_e8 == 222);

    // A record with the 0xff "no index" sentinel.
    img[1 + 0xc9] = 0xff;
    assert(parse_spec(img, t));
    assert(t.records[0].index_c9 == 0xff);
    assert(!t.records[0].has_index());

    // Truncated buffers must be rejected, not read out of bounds.
    SpecTable bad;
    std::vector<uint8_t> trunc(1 + kSpecRecordSize - 1, 0);
    trunc[0] = 1;
    assert(!parse_spec(trunc, bad));
    assert(!bad.ok && bad.records.empty());

    std::vector<uint8_t> empty;
    assert(!parse_spec(empty, bad));   // no count byte at all

    // A count byte of 0 is valid and yields no records.
    std::vector<uint8_t> none(1, 0);
    assert(parse_spec(none, bad));
    assert(bad.ok && bad.records.empty());

    // Dispatch-type names (oracle-derived): the handler set the disassembly
    // keys off must all have stable labels, and unknown values must not crash.
    assert(std::string(spec_type_name(0)) == "text");
    assert(std::string(spec_type_name(1)) == "recruit");
    assert(std::string(spec_type_name(7)) == "door");
    assert(std::string(spec_type_name(9)) == "sign");
    assert(std::string(spec_type_name(0x0c)) == "button");
    assert(std::string(spec_type_name(0x0e)) == "challenge");
    assert(std::string(spec_type_name(0x11)) == "puzzle");
    assert(std::string(spec_type_name(0x19)) == "elemental");
    assert(std::string(spec_type_name(9999)) == "unknown");

    printf("char_test: synthetic SPEC ok\n");
}

// Verify the real data, when present.  Verifies record counts and a known
// string from SPEC0 and a known roster name.
static int real_data() {
    const char* dir = "game";
    std::string p0 = std::string(dir) + "/SYST/SPEC0";
    FILE* f = fopen(p0.c_str(), "rb");
    if (!f) {
        printf("char_test: game data absent, skipping real-data checks\n");
        return 0;
    }
    fclose(f);

    SpecTable t = load_spec(p0);
    assert(t.ok);
    assert(t.declared_count == 24);
    assert(t.records.size() == 24);
    // SPEC0[0] is the Woodkeeper recruit (verified against the game file and
    // the oracle's own event dispatcher, which prints text_b at +0x50).
    assert(t.records[0].text_a == "He shakes his head.");
    assert(t.records[0].text_b.rfind("A Woodkeeper plants himself", 0) == 0);
    assert(t.records[0].type == SPEC_TYPE_RECRUIT);

    // SPEC0[15..18] are the four elemental guardians (type 25); the engine's
    // 0x19 handler reads index_c9 (+0xc9) to pick which one.  The observed
    // indices 6,7,8,9 correspond to Water, Earth, Air, Fire in text order.
    assert(t.records[15].type == SPEC_TYPE_ELEMENTAL);
    assert(t.records[15].index_c9 == 6);
    assert(t.records[15].text_b.rfind("A Water Elemental appears", 0) == 0);
    assert(t.records[16].index_c9 == 7);
    assert(t.records[17].index_c9 == 8);
    assert(t.records[18].type == SPEC_TYPE_ELEMENTAL);
    assert(t.records[18].index_c9 == 9);
    assert(t.records[18].text_b.rfind("A Fire Elemental appears", 0) == 0);

    // Every record's type lies in the observed set 0..25 (no gaps above 22
    // except 23; 23 never appears in this release).
    for (const auto& r : t.records)
        assert(r.type <= 25 && r.type != (SpecType)23);

    // 665 records across the eight scenario files (stable for this release).
    size_t total = 0, unset_4e = 0;
    for (int n = 0; n <= 7; n++) {
        char p[64];
        snprintf(p, sizeof p, "%s/SYST/SPEC%d", dir, n);
        SpecTable tt = load_spec(p);
        assert(tt.ok);
        total += tt.records.size();
        for (const auto& r : tt.records)
            if (r.arg_4e < 0) unset_4e++;  // -1 == "unset" (engine handler
                                           // gates the text window on it)
    }
    assert(total == 665);
    // arg_4e (offset 0x4e, read signed by the event dispatcher) is -1 in all
    // but six records in this release.
    assert(unset_4e == 659);

    // SPEC6 has 182 records; SPEC7 has 41 (the counts are stable per file).
    SpecTable t6 = load_spec(std::string(dir) + "/SYST/SPEC6");
    assert(t6.ok && t6.records.size() == 182);
    SpecTable t7 = load_spec(std::string(dir) + "/SYST/SPEC7");
    assert(t7.ok && t7.records.size() == 41);

    auto roster = load_rost(std::string(dir) + "/ROST/ROST");
    assert(roster.size() == 11);
    assert(roster[0].name == "Connar");
    assert(roster[10].name == "Siegurd");
    assert(roster[0].present == 1);
    // The 4-byte trailing value the loader reads separately (410..413).
    assert(roster[0].tail == 0x68a);
    assert(roster[10].tail == 0x592);
    for (size_t i = 0; i < roster.size(); i++)
        assert(roster[i].present == 1);  // all 11 slots hold a character

    ShopData shop = load_shop(std::string(dir) + "/ROST/SHOP");
    assert(shop.ok);
    assert(shop.adjustments.size() == 45);
    assert(shop.adjustments[0] == -10);
    assert(shop.adjustments[1] == -2);
    assert(shop.trailing == 0x1606);

    // Saved games ("gamestandings"): header 0x00 'F' 'O' 'L' + 410-byte
    // character records.  The engine opens GAMES\GAME%d (string in LOF.EXE).
    SaveGame sv = load_save(std::string(dir) + "/GAMES/GAME1");
    assert(sv.ok);
    assert(sv.header[0] == 0x00 && sv.header[1] == 'F' &&
           sv.header[2] == 'O' && sv.header[3] == 'L');
    assert(sv.chars.size() == 32);
    assert(sv.chars[0].present == 1 && sv.chars[0].name == "Decker");
    assert(sv.chars[1].name == "Siegurd");
    assert(sv.chars[5].name == "Assanla");
    assert(sv.trailer.size() == 10);

    // Character-struct offsets the engine's init/erase routines use directly
    // (analysis/lof.asm 1B9C2..1BA7E).  Live slots carry word10 == -1.
    assert((int16_t)(sv.chars[0].raw[kCharFlags10] |
                     (sv.chars[0].raw[kCharFlags10 + 1] << 8)) == -1);
    assert(sv.chars[0].raw[kCharStat1d] == 0x0a);
    assert(sv.chars[0].raw[kCharLevel46] == 0x0e);

    // Item table (transcribed from LOF.EXE; no data file carries it).
    const auto& it = items();
    assert(it.size() == kItemCount);
    assert(kItemCount == 185);
    assert(it[0].name == "Club" && it[0].item_class == 1);
    assert(it[12].name == "Short sword" && it[12].f0 == 1 &&
           it[12].f1 == 10 && it[12].f2 == 50);
    assert(it[0x1a].name == "Leather armour" && it[0x1a].item_class == 2);
    assert(it[184].name == "The Staff");
    // 1-based lookup: index 13 is "Short sword"; out-of-range is null.
    assert(item_by_index(13) && item_by_index(13)->name == "Short sword");
    assert(item_by_index(185) && item_by_index(185)->name == "The Staff");
    assert(item_by_index(0) == nullptr && item_by_index(186) == nullptr);
    // Every class-eligibility mask fits the 14 SNSC classes.
    for (const auto& r : it) assert(r.f6 >= 0 && r.f6 <= 0x3fff);

    printf("char_test: real SPEC/ROST/SHOP/GAMES/items ok\n");
    return 0;
}

int main(int argc, char** argv) {
    synthetic_spec();

    // --dump-spec prints the decoded tables (for eyeballing / the front-end).
    if (argc > 1 && std::string(argv[1]) == "--dump-spec") {
        for (int n = 0; n <= 7; n++) {
            char p[64];
            snprintf(p, sizeof p, "game/SYST/SPEC%d", n);
            SpecTable t = load_spec(p);
            if (t.ok) {
                char label[64];
                snprintf(label, sizeof label, "SYST/SPEC%d", n);
                dump_spec(stdout, t, label);
            }
        }
        return 0;
    }

    if (argc > 1 && std::string(argv[1]) == "--dump-items") {
        dump_items(stdout);
        return 0;
    }

    int rc = real_data();
    if (rc == 0) printf("char_test: ok\n");
    return rc;
}
