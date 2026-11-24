#include "char.h"

#include <cstdio>
#include <cstring>

#include "data.h"

namespace lofport {

#include "char_items.inc"
#include "char_spells.inc"
#include "char_magic.inc"
#include "char_names.inc"

namespace {

std::vector<uint8_t> read_file(const std::string& path) {
    std::vector<uint8_t> v;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return v;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n > 0) {
        v.resize(n);
        if (fread(v.data(), 1, n, f) != (size_t)n) v.clear();
    }
    fclose(f);
    return v;
}

uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
uint32_t rd32(const uint8_t* p) {
    return (uint32_t)(p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

// Read a NUL-terminated string from `p`, at most `limit` bytes.
std::string cstr(const uint8_t* p, size_t limit) {
    size_t l = 0;
    while (l < limit && p[l]) l++;
    return std::string((const char*)p, l);
}

}  // namespace

// Case-insensitive keyword compare, matching the engine's folding at 0x1BE9F.
// The engine consults a case table and subtracts 0x20 from characters marked as
// lower case; the shipped keywords are all ASCII, so a plain tolower on ASCII is
// equivalent.  The comparison walks both strings to their NUL and requires equal
// length, exactly as 0x1BE70 does.
bool keyword_matches(const std::string& keyword, const std::string& answer) {
    if (keyword.empty()) return false;
    size_t i = 0;
    for (; i < keyword.size() && i < answer.size(); i++) {
        unsigned char a = (unsigned char)keyword[i];
        unsigned char b = (unsigned char)answer[i];
        if (a >= 'a' && a <= 'z') a = (unsigned char)(a - 0x20);
        if (b >= 'a' && b <= 'z') b = (unsigned char)(b - 0x20);
        if (a != b) return false;
    }
    return i == keyword.size() && i == answer.size();
}

DoorTarget door_target(const SpecRecord& r) {
    DoorTarget t;
    t.location = r.counter;
    t.map = (int)(int16_t)r.arg_e6;
    t.cell = r.arg_e8;
    t.pending = true;
    return t;
}

bool CoffinLatch::visit(uint16_t type) {
    if (type != 16) return false;
    if (armed) {
        armed = false;
        return true;
    }
    armed = true;
    return false;
}

AnswerEffect classify_answer(const SpecRecord& r, const std::string& answer) {
    if (r.keyword.empty()) return AnswerEffect::NoAnswerWanted;
    return keyword_matches(r.keyword, answer) ? AnswerEffect::Match
                                             : AnswerEffect::Mismatch;
}

bool answer_prints_outcome(const SpecRecord& r, AnswerEffect effect) {
    if (effect == AnswerEffect::NoAnswerWanted) return false;
    // Both branches gate on `+0x4b == 0` and then on `+0x0e != 0`.
    return r.flag_4b == 0 && !r.text_a.empty();
}

bool answer_fires_effect(AnswerEffect effect) {
    // A record with no keyword jumps straight to the type switch
    // (0x209CE: jmp 0x20AB8), so its effect fires unconditionally; only a
    // wrong answer suppresses it.
    return effect != AnswerEffect::Mismatch;
}

bool parse_spec(const std::vector<uint8_t>& data, SpecTable& out) {
    out = SpecTable{};
    if (data.empty()) return false;
    const size_t count = data[0];
    if (data.size() < 1 + count * kSpecRecordSize) return false;

    out.declared_count = (uint8_t)count;
    out.records.reserve(count);
    for (size_t i = 0; i < count; i++) {
        const uint8_t* r = data.data() + 1 + i * kSpecRecordSize;
        SpecRecord rec;
        std::memcpy(rec.raw.data(), r, kSpecRecordSize);

        rec.keyword = cstr(r + 0x00, 0x0e);
        rec.text_a = cstr(r + 0x0e, 0x4a - 0x0e);
        rec.text_b = cstr(r + 0x50, 0xc6 - 0x50);

        rec.flag_4b = r[0x4b];
        rec.arg_4c = rd16(r + 0x4c);
        rec.arg_4e = (int16_t)rd16(r + 0x4e);
        rec.index_c9 = r[0xc9];
        for (int i = 0; i < 4; i++) rec.args[i] = rd16(r + 0xda + 2 * i);
        rec.arg_da = rec.args[0];
        rec.type = (SpecType)rd16(r + 0xe2);
        rec.counter = rd16(r + 0xe4);
        rec.arg_e6 = rd16(r + 0xe6);
        rec.arg_e8 = rd16(r + 0xe8);
        rec.class_eb = r[0xeb];

        out.records.push_back(std::move(rec));
    }
    out.ok = true;
    return true;
}

SpecTable load_spec(const std::string& path) {
    SpecTable t;
    parse_spec(read_file(path), t);
    return t;
}

uint32_t rost_record_checksum(const uint8_t* rec410) {
    // Alternating ADD (even index) / XOR (odd index) fold seeded with 1; NUL
    // bytes count as 1.  See docs/notes-maintainer-rost.md (asm 0x1BF18).
    uint32_t acc = 1;
    for (size_t i = 0; i < 410; i++) {
        uint32_t c = rec410[i] ? rec410[i] : 1;
        acc = (i % 2 == 0) ? (acc + c) : (acc ^ c);
    }
    return acc;
}

std::vector<RostRecord> load_rost(const std::string& path) {
    std::vector<RostRecord> out;
    std::vector<uint8_t> d = read_file(path);
    if (d.empty()) return out;
    const size_t count = d[0];
    if (d.size() < 1 + count * kRostRecordSize) return out;
    out.reserve(count);
    for (size_t i = 0; i < count; i++) {
        const uint8_t* r = d.data() + 1 + i * kRostRecordSize;
        RostRecord rec;
        std::memcpy(rec.raw.data(), r, kRostRecordSize);
        rec.present = r[0x00];
        rec.name = cstr(r + 0x01, 14);
        rec.tail = (uint32_t)r[410] | ((uint32_t)r[411] << 8) |
                   ((uint32_t)r[412] << 16) | ((uint32_t)r[413] << 24);
        rec.intact = (rec.tail == rost_record_checksum(rec.raw.data()));
        out.push_back(std::move(rec));
    }
    return out;
}

SaveGame load_save(const std::string& path) {
    SaveGame g;
    std::vector<uint8_t> d = read_file(path);
    if (d.size() < kSaveHeaderSize) return g;
    std::memcpy(g.header, d.data(), kSaveHeaderSize);
    g.ok = std::memcmp(g.header, kSaveMagic, 4) == 0;
    if (!g.ok) return g;
    if (d.size() > kSaveLocOnes) g.loc_ones = d[kSaveLocOnes];
    if (d.size() > kSaveLocTens) g.loc_tens = d[kSaveLocTens];
    if (d.size() > kSavePartyPos + 1) {
        g.party_pos = (uint16_t)((uint16_t)d[kSavePartyPos] |
                                 ((uint16_t)d[kSavePartyPos + 1] << 8));
        // A cell index past the 36x36 grid cannot be a position; treat it as
        // absent so the caller falls back to the level's entry cell.
        if (g.party_pos >= 36 * 36) g.party_pos = kSavePartyPosInvalid;
    }
    if (d.size() > kSaveFacing) g.facing = d[kSaveFacing];
    // Both fields are dwords in the save: the loader copies 0x9b6 -> global
    // 0x3b0c and 0x9ba -> global 0x3b10 (image 0x1ca00 / 0x1ca1a, two 16-bit
    // moves each).  The status box indexes the tables by the low word.
    if (d.size() > kSaveTimeOfDay + 3) {
        uint32_t t = (uint32_t)d[kSaveTimeOfDay] |
                     ((uint32_t)d[kSaveTimeOfDay + 1] << 8) |
                     ((uint32_t)d[kSaveTimeOfDay + 2] << 16) |
                     ((uint32_t)d[kSaveTimeOfDay + 3] << 24);
        g.time_of_day = (int)t;
    }
    if (d.size() > kSaveMorale + 3) {
        uint32_t m = (uint32_t)d[kSaveMorale] |
                     ((uint32_t)d[kSaveMorale + 1] << 8) |
                     ((uint32_t)d[kSaveMorale + 2] << 16) |
                     ((uint32_t)d[kSaveMorale + 3] << 24);
        g.morale = (int)m;
    }
    // Exactly six records; the world state begins where they end, so parsing to
    // EOF would swallow it (see the layout comment in the header).
    size_t n = std::min(kSaveCharCount, (d.size() - kSaveHeaderSize) / 410);
    for (size_t i = 0; i < n; i++) {
        const uint8_t* r = d.data() + kSaveHeaderSize + i * 410;
        SaveChar c;
        std::memcpy(c.raw.data(), r, 410);
        c.present = r[0];
        c.profession = r[0x1b];
        c.panel_stat = r[0x1e];
        std::string nm;
        for (int k = 1; k < 15 && r[k]; k++) nm.push_back((char)r[k]);
        c.name = std::move(nm);
        g.chars.push_back(std::move(c));
    }
    if (d.size() > kSaveCharsEnd) {
        size_t end = std::min(d.size(), kSaveChecksum);
        g.world.assign(d.begin() + kSaveCharsEnd, d.begin() + end);
    }
    if (d.size() > kSaveMap)
        g.map.assign(d.begin() + kSaveMap, d.end());
    g.sealed = save_checksum_ok(d);
    return g;
}

uint32_t save_checksum(const uint8_t* data, size_t len) {
    // Identical fold to the ROST record trailer: seed 1, add on even index, xor
    // on odd, NUL counted as 1.
    uint32_t acc = 1;
    for (size_t i = 0; i < len; i++) {
        uint32_t c = data[i] ? data[i] : 1;
        acc = (i % 2 == 0) ? (acc + c) : (acc ^ c);
    }
    return acc;
}

std::vector<uint8_t> build_save(const SaveGame& g) {
    std::vector<uint8_t> out(kSaveSize, 0);
    std::memcpy(out.data(), g.header, kSaveHeaderSize);
    // Records: only the slots the caller actually holds.  A short party leaves
    // the remaining slots zeroed, which is what an empty slot looks like.
    for (size_t i = 0; i < g.chars.size() && i < kSaveCharCount; i++)
        std::memcpy(out.data() + kSaveHeaderSize + i * 410,
                    g.chars[i].raw.data(), 410);
    // World state, then the per-cell array, copied verbatim from the load.
    for (size_t i = 0; i < g.world.size() && kSaveCharsEnd + i < kSaveChecksum; i++)
        out[kSaveCharsEnd + i] = g.world[i];
    for (size_t i = 0; i < g.map.size() && kSaveMap + i < kSaveSize; i++)
        out[kSaveMap + i] = g.map[i];
    // Re-seal: the engine rejects a save whose trailer does not fold correctly.
    uint32_t sum = save_checksum(out.data(), kSaveChecksum);
    for (int i = 0; i < 4; i++)
        out[kSaveChecksum + i] = (uint8_t)((sum >> (8 * i)) & 0xff);
    return out;
}

bool save_checksum_ok(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < kSaveChecksum + 4) return false;
    uint32_t stored = (uint32_t)bytes[kSaveChecksum] |
                      ((uint32_t)bytes[kSaveChecksum + 1] << 8) |
                      ((uint32_t)bytes[kSaveChecksum + 2] << 16) |
                      ((uint32_t)bytes[kSaveChecksum + 3] << 24);
    return stored == save_checksum(bytes.data(), kSaveChecksum);
}

bool write_save_file(const std::string& path, const std::vector<uint8_t>& bytes) {
    if (bytes.size() != kSaveSize) return false;
    if (!save_checksum_ok(bytes)) return false;   // never write an unsealed save
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    size_t n = std::fwrite(bytes.data(), 1, bytes.size(), f);
    return std::fclose(f) == 0 && n == bytes.size();
}

// Offset of a world-state field inside SaveGame::world.
static size_t world_off(size_t file_off) { return file_off - kSaveCharsEnd; }

void save_set_party(SaveGame& g, uint16_t cell, uint8_t facing) {
    g.party_pos = cell;
    g.facing = facing;
    if (g.world.size() > world_off(kSaveFacing)) {
        g.world[world_off(kSavePartyPos)] = (uint8_t)(cell & 0xff);
        g.world[world_off(kSavePartyPos) + 1] = (uint8_t)(cell >> 8);
        g.world[world_off(kSaveFacing)] = facing;
    }
}

void save_set_location(SaveGame& g, uint8_t tens, uint8_t ones) {
    g.loc_tens = tens;
    g.loc_ones = ones;
    if (g.world.size() > world_off(kSaveLocTens)) {
        g.world[world_off(kSaveLocOnes)] = ones;
        g.world[world_off(kSaveLocTens)] = tens;
    }
}

int give_item(SaveChar& c, int item_index) {
    const ItemRecord* it = item_by_index(item_index);
    if (!it) return -1;
    int slot = -1;
    for (int i = 0; i < (int)kCharInvCount; ++i) {
        if (c.raw[kCharInv82 + i * 4] == 0) { slot = i; break; }
    }
    if (slot < 0) return -1;
    uint8_t* p = &c.raw[kCharInv82 + slot * 4];
    p[0] = (uint8_t)item_index;   // item index (image 0x6a4d)
    p[2] = it->charges;           // charges from item +0x13 (0x6a8e / 0x6ae2)
    p[3] = 100;                   // condition = 0x64 (0x6aad)
    c.raw[kCharInvUsed6b] += 1;   // used-slot count (0x6c2a)
    // Refresh the carried total the way the post-add recompute does.
    CharSheet sh = decode_char(c.raw.data());
    uint16_t w = (uint16_t)(weight_base(sh) + carried_item_weight(sh));
    c.raw[kCharWeight66]     = (uint8_t)(w & 0xff);
    c.raw[kCharWeight66 + 1] = (uint8_t)(w >> 8);
    return slot;
}

static uint16_t rd16(const uint8_t* r, int off) {
    return (uint16_t)(r[off] | (r[off + 1] << 8));
}
static void wr16(uint8_t* r, int off, uint16_t v) {
    r[off] = (uint8_t)(v & 0xff);
    r[off + 1] = (uint8_t)(v >> 8);
}
static uint32_t rd32(const uint8_t* r, int off) {
    return (uint32_t)r[off] | ((uint32_t)r[off + 1] << 8) |
           ((uint32_t)r[off + 2] << 16) | ((uint32_t)r[off + 3] << 24);
}
static void wr32(uint8_t* r, int off, uint32_t v) {
    r[off] = (uint8_t)(v & 0xff);
    r[off + 1] = (uint8_t)((v >> 8) & 0xff);
    r[off + 2] = (uint8_t)((v >> 16) & 0xff);
    r[off + 3] = (uint8_t)((v >> 24) & 0xff);
}

int rest_cost(const SaveGame& g, bool with_rations) {
    int members = 0;
    long sum = 0;
    for (const SaveChar& c : g.chars) {
        if (c.raw[0] == 0) continue;
        // (wmax - wcur) is an unsigned 16-bit divide in the engine (0x827b).
        uint16_t spare = (uint16_t)((uint16_t)(rd16(c.raw.data(), kCharWeight64) -
                                               rd16(c.raw.data(), kCharWeight66)) / 10);
        if (spare > 14) spare = 14;
        sum += spare;
        members++;
    }
    int cost = 2 * members;
    if (with_rations) cost += (int)(3 * sum);
    return cost;
}

bool pay_gold(SaveChar& c, int cost) {
    if (cost < 0) cost = 0;
    uint32_t gold = (uint32_t)c.raw[kCharGold72]
                  | ((uint32_t)c.raw[kCharGold72 + 1] << 8)
                  | ((uint32_t)c.raw[kCharGold72 + 2] << 16)
                  | ((uint32_t)c.raw[kCharGold72 + 3] << 24);
    if (gold < (uint32_t)cost) return false;
    gold -= (uint32_t)cost;
    c.raw[kCharGold72]     = (uint8_t)(gold & 0xff);
    c.raw[kCharGold72 + 1] = (uint8_t)((gold >> 8) & 0xff);
    c.raw[kCharGold72 + 2] = (uint8_t)((gold >> 16) & 0xff);
    c.raw[kCharGold72 + 3] = (uint8_t)((gold >> 24) & 0xff);
    // Gold weighs 0.1 lb each, so refresh the carried total (+0x66).
    CharSheet sh = decode_char(c.raw.data());
    uint16_t w = (uint16_t)(weight_base(sh) + carried_item_weight(sh));
    wr16(c.raw.data(), kCharWeight66, w);
    return true;
}

void rest_party(SaveGame& g, bool with_rations) {
    for (SaveChar& c : g.chars) {
        if (c.raw[0] == 0) continue;
        uint8_t* r = c.raw.data();
        if ((int8_t)r[kCharOutOfAction49] > 0) {
            r[kCharOutOfAction49]--;
            if (r[kCharOutOfAction49] == 0) {
                r[kCharStatus1f] = 0;
                r[0x4b] = 0;
            }
        }
        if (with_rations) r[kCharRations70] = 14;
        if (r[kCharStatus1f] == 0) {
            int hp = rd16(r, kCharHpCur22);
            hp += 2 * r[kCharLevel17];
            if (r[kCharCon44] > 14) hp += r[kCharCon44] - 14;
            int hp_max = rd16(r, kCharHpMax20);
            if (hp > hp_max) hp = hp_max;
            wr16(r, kCharHpCur22, (uint16_t)hp);
        }
        if (r[kCharMagMax68] != 0 && r[kCharMagCur69] < r[kCharMagMax68]) {
            int gain = r[kCharMagMax68] >> 3;
            if (gain < 1) gain = 1;
            int cur = r[kCharMagCur69] + gain;
            if (cur > r[kCharMagMax68]) cur = r[kCharMagMax68];
            r[kCharMagCur69] = (uint8_t)cur;
        }
    }
}

int temple_heal_cost(const SaveChar& c) {
    const uint8_t* r = c.raw.data();
    int missing = rd16(r, kCharHpMax20) - rd16(r, kCharHpCur22);
    if (missing <= 0) return 0;
    long cost = (long)missing * r[kCharLevel17] / 2;
    return cost < 1 ? 1 : (int)cost;
}

void temple_heal_apply(SaveChar& c) {
    uint8_t* r = c.raw.data();
    wr16(r, kCharHpCur22, rd16(r, kCharHpMax20));  // hp_cur = hp_max
    if (r[kCharStatus1f] != 0) {                    // cure condition (0xab41)
        r[kCharStatus1f] = 0;
        r[0x55] = 1;
    }
}

int temple_revive_cost(const SaveChar& c) {
    return (int)c.raw[kCharLevel17] * 300;
}

bool temple_revive(SaveChar& c, int roll) {
    uint8_t* r = c.raw.data();
    int con = r[kCharCon44];
    bool ok = con != 0 && con * 4 + 5 >= roll;
    if (ok) {
        if (con > 1) r[kCharCon44] = (uint8_t)(con - 1);
        r[kCharStatus1f] = 0;
        if (rd16(r, kCharHpCur22) < 1) wr16(r, kCharHpCur22, 1);
    } else {
        wr16(r, kCharHpMax20, 0);
        wr16(r, kCharHpCur22, 0);
    }
    return ok;
}

bool bank_deposit(SaveChar& c, uint32_t amount) {
    uint8_t* r = c.raw.data();
    uint32_t gold = rd32(r, kCharGold72);
    if (gold < amount) return false;
    wr32(r, kCharGold72, gold - amount);
    wr32(r, kCharBank76, rd32(r, kCharBank76) + amount);
    CharSheet sh = decode_char(c.raw.data());
    wr16(r, kCharWeight66, (uint16_t)(weight_base(sh) + carried_item_weight(sh)));
    return true;
}

bool bank_withdraw(SaveChar& c, uint32_t amount) {
    uint8_t* r = c.raw.data();
    uint32_t bank = rd32(r, kCharBank76);
    if (bank < amount) return false;
    wr32(r, kCharBank76, bank - amount);
    wr32(r, kCharGold72, rd32(r, kCharGold72) + amount);
    CharSheet sh = decode_char(c.raw.data());
    wr16(r, kCharWeight66, (uint16_t)(weight_base(sh) + carried_item_weight(sh)));
    return true;
}

uint32_t char_gold(const SaveChar& c) { return rd32(c.raw.data(), kCharGold72); }
uint32_t char_bank(const SaveChar& c) { return rd32(c.raw.data(), kCharBank76); }

bool buy_item(SaveGame& g, int char_idx, int item_index, ShopData* shop) {
    if (char_idx < 0 || char_idx >= (int)g.chars.size() || g.chars[char_idx].present == 0)
        return false;
    const ItemRecord* it = item_by_index(item_index);
    if (!it) return false;
    if (shop && shop->ok && !shop->carries(item_index)) return false;
    int price = it->price < 0 ? 0 : it->price;
    SaveChar& c = g.chars[char_idx];
    if (!pay_gold(c, price)) return false;
    if (give_item(c, item_index) < 0) {
        uint8_t* r = c.raw.data();
        wr32(r, kCharGold72, rd32(r, kCharGold72) + (uint32_t)price);
        CharSheet sh = decode_char(c.raw.data());
        wr16(r, kCharWeight66, (uint16_t)(weight_base(sh) + carried_item_weight(sh)));
        return false;
    }
    if (shop && shop->ok && !shop->unlimited(item_index) && shop->stock[item_index - 1] > 0)
        shop->stock[item_index - 1]--;
    return true;
}

std::string apply_spell_effect(SaveChar& t, const SpellRecord& sp) {
    uint8_t* r = t.raw.data();
    switch (sp.data[kSpellEffectByte]) {
    case 15: {  // heal = spell block byte 6, capped at hp_max (handler 0xf848)
        uint16_t hp = rd16(r, kCharHpCur22), mx = rd16(r, kCharHpMax20);
        uint32_t nh = (uint32_t)hp + (uint32_t)sp.data[6];
        if (nh > mx) nh = mx;
        wr16(r, kCharHpCur22, (uint16_t)nh);
        return "heals";
    }
    case 1: {  // drain: hp_max -= 5 (min 0), clamp hp_cur (handler 0xf5f2)
        int mx = (int)(int16_t)rd16(r, kCharHpMax20) - 5;
        if (mx < 0) mx = 0;
        wr16(r, kCharHpMax20, (uint16_t)mx);
        if ((int16_t)rd16(r, kCharHpCur22) > mx)
            wr16(r, kCharHpCur22, (uint16_t)mx);
        return "drains";
    }
    case 14:  // set the target's +0x12 word to 6 (handler 0xf7f6)
        wr16(r, 0x12, 6);
        return "affects";
    default:
        return "";
    }
}

std::vector<SpellSlot> spell_slots(const SaveChar& c) {
    std::vector<SpellSlot> out;
    for (int n = 0; n < 4; n++) {
        int idx = c.raw[kCharSkill142 + 2 * n];
        if (idx == 0) continue;
        out.push_back({idx, c.raw[kCharSkill142 + 2 * n + 1]});
    }
    return out;
}

void consume_spell_slot(SaveChar& c, int slot) {
    if (slot < 0 || slot > 3) return;
    int lo = kCharSkill142 + 2 * slot;
    if (c.raw[lo] == 0) return;             // empty slot: nothing to consume
    if (c.raw[lo + 1] > 0) c.raw[lo + 1]--;  // dec remaining casts
    if (c.raw[lo + 1] == 0) c.raw[lo] = 0;   // forgotten when exhausted
}

LevelUpResult level_up(SaveChar& c, int hp_gain) {
    LevelUpResult res;
    uint8_t* r = c.raw.data();
    auto add_word = [](uint8_t* p, int v) {
        int w = p[0] | (p[1] << 8);
        w += v;
        p[0] = (uint8_t)(w & 0xff);
        p[1] = (uint8_t)((w >> 8) & 0xff);
    };
    r[0x17]++;
    res.new_level = r[0x17];
    res.hp_gain = hp_gain;
    add_word(&r[0x22], hp_gain);   // current HP (+0x22)
    add_word(&r[0x20], hp_gain);   // max HP (+0x20)
    for (int off : {0x30, 0x32, 0x34, 0x36})
        if (r[off] < 0x64) r[off]++;
    struct { int off; const char* name; } attrs[] = {
        {0x45, "Strength"}, {0x46, "Dexterity"}, {0x48, "Wisdom"}};
    for (auto& a : attrs)
        if (r[a.off] < 0x13) { r[a.off]++; res.improved.push_back(a.name); }
    return res;
}

std::string use_item_effect(SaveChar& c, int item_index) {
    const ItemRecord* it = item_by_index(item_index);
    if (!it) return "";
    uint8_t* r = c.raw.data();
    int val = it->charges;
    switch (it->effect) {
    case 10: {  // heal HP; a healing spell's block byte 6 is the amount
        // The engine's action-3 "use object" casts the item's spell (item +0x18);
        // the effect-15 (healing) handler adds spell block byte 6 (handler
        // 0xf848).  Fall back to the item's charge count only when the item
        // casts no healing spell.
        const SpellRecord* sp = item_spell(item_index);
        if (sp && sp->data[kSpellEffectByte] == 15)
            return apply_spell_effect(c, *sp);
        uint16_t hp = rd16(r, kCharHpCur22), mx = rd16(r, kCharHpMax20);
        uint32_t nh = (uint32_t)hp + (uint32_t)val;
        if (nh > mx) nh = mx;
        wr16(r, kCharHpCur22, (uint16_t)nh);
        return "heals";
    }
    case 3:  // poison: hp -= charges, mark the poisoned state
        wr16(r, kCharHpCur22, (uint16_t)(rd16(r, kCharHpCur22) - (uint16_t)val));
        r[kCharStatus1f] = 3;
        r[0x57] = 4;
        r[0x58] = 0x12;
        return "poisons";
    case 1:  // enchant weapon (+0x5e), refuses past +0x5e + +0x45 >= 0x11
        if ((int)r[0x5e] + (int)r[0x45] < 0x11) { r[0x5e] += 2; return "enchants the weapon"; }
        return "finds the weapon already enchanted";
    case 2:  // enchant armour (+0x5b)
        r[0x5b] += 2;
        return "enchants the armour";
    case 30:  // cure poison (state 3 -> clear +0x1f/+0x57/+0x58)
        if (r[kCharStatus1f] == 3) {
            r[kCharStatus1f] = 0; r[0x57] = 0; r[0x58] = 0;
            return "cures the poison";
        }
        return "finds no poison to cure";
    case 31:  // cure illness (state 4 -> clear +0x1f)
        if (r[kCharStatus1f] == 4) { r[kCharStatus1f] = 0; return "cures the illness"; }
        return "finds no illness to cure";
    default:
        return "";
    }
}

bool joint_deposit(SaveGame& g, int target) {
    uint32_t total = 0;
    for (auto& c : g.chars)
        if (c.present) total += char_gold(c);
    if (total == 0) return false;
    if (target < 0 || target >= (int)g.chars.size() || g.chars[target].present == 0)
        return false;
    SaveChar& t = g.chars[target];
    uint8_t* tr = t.raw.data();
    long after = (long)rd16(tr, kCharWeight66) - (long)char_gold(t) + (long)total;
    if (after > (long)rd16(tr, kCharWeight64)) return false;
    for (auto& c : g.chars) {
        if (!c.present) continue;
        wr32(c.raw.data(), kCharGold72, 0);
        CharSheet s = decode_char(c.raw.data());
        wr16(c.raw.data(), kCharWeight66, (uint16_t)(weight_base(s) + carried_item_weight(s)));
    }
    wr32(t.raw.data(), kCharGold72, total);
    CharSheet s = decode_char(t.raw.data());
    wr16(t.raw.data(), kCharWeight66, (uint16_t)(weight_base(s) + carried_item_weight(s)));
    return true;
}

uint16_t save_location(const SaveGame& g) {
    return (uint16_t)(g.loc_tens * 10 + g.loc_ones);
}

std::string dungeon_map_name(uint16_t location) {
    if (location > 99) return {};
    char buf[16];
    std::snprintf(buf, sizeof buf, "DNG%01u%01u", location / 10, location % 10);
    return buf;
}

std::string dungeon_view_dir(uint16_t location) {
    unsigned ones = location % 10;
    if (location > 99 || ones > 7) return {};
    return std::string("D") + (char)('0' + ones);
}

ShopData load_shop(const std::string& path) {
    ShopData s;
    std::vector<uint8_t> d = read_file(path);
    if (d.empty()) return s;
    // Byte 0 is a header (zero in this release); the next 185 bytes are the
    // per-item stock counts, parallel to the item table order (the engine reads
    // 186 bytes then a 4-byte trailer).  See ShopData for the value semantics.
    if (d.size() < 1 + kItemCount) return s;
    for (size_t i = 0; i < kItemCount; i++) s.stock[i] = d[1 + i];
    if (d.size() >= 4) s.trailer = rd16(d.data() + d.size() - 4);
    s.ok = true;
    return s;
}

const char* spec_type_name(uint16_t type) {
    switch (type) {
        case 0: return "text";
        case 1: return "recruit";
        case 2: return "spring";
        case 3: return "spring2";
        case 4: return "battle";
        case 5: return "passage";
        case 6: return "empty6";
        case 7: return "door";
        case 8: return "trade";
        case 9: return "sign";
        case 10: return "spring3";
        case 11: return "teleport";
        case 12: return "button";
        case 13: return "inscription";
        case 14: return "challenge";
        case 15: return "empty15";
        case 16: return "lava";
        case 17: return "puzzle";
        case 18: return "trap";
        case 19: return "button19";
        case 20: return "button20";
        case 21: return "button21";
        case 22: return "button22";
        case 24: return "door24";
        case 25: return "elemental";
        default: return "unknown";
    }
}

void dump_spec(std::FILE* out, const SpecTable& t, const char* label) {
    std::fprintf(out, "%s: %u records\n", label, (unsigned)t.declared_count);
    for (size_t i = 0; i < t.records.size(); i++) {
        const SpecRecord& r = t.records[i];
        std::fprintf(out,
                     "  [%3zu] type=%-3u (%-11s) idx=%02x cls=%02x arg4c=%u "
                     "arg4e=%d cnt=%u\n",
                     i, (unsigned)r.type, spec_type_name(r.type), r.index_c9,
                     r.class_eb, r.arg_4c, r.arg_4e, r.counter);
        if (!r.keyword.empty()) std::fprintf(out, "        kw: %s\n", r.keyword.c_str());
        if (!r.text_a.empty()) std::fprintf(out, "        a: %s\n", r.text_a.c_str());
        if (!r.text_b.empty()) std::fprintf(out, "        b: %s\n", r.text_b.c_str());
    }
}

const std::vector<ItemRecord>& items() {
    static const std::vector<ItemRecord> table = [] {
        std::vector<ItemRecord> v;
        v.reserve(kItemCount);
        for (size_t i = 0; i < kItemCount; i++) {
            ItemRecord r;
            r.name = kItemNames[i];
            r.f0 = kItemFields[i][0];
            r.price = kItemFields[i][1];
            r.weight = kItemFields[i][2];
            r.f3 = kItemFields[i][3];
            r.charges = static_cast<uint8_t>((kItemFields[i][3] >> 8) & 0xff);
            r.f4 = kItemFields[i][4];
            r.spell = static_cast<uint8_t>(kItemFields[i][4] & 0xff);
            r.damage = static_cast<uint8_t>((kItemFields[i][4] >> 8) & 0xff);
            r.f5 = kItemFields[i][5];
            r.f6 = kItemFields[i][6];
            r.item_class = kItemClassEffect[i][0];
            r.effect = kItemClassEffect[i][1];
            v.push_back(std::move(r));
        }
        return v;
    }();
    return table;
}

const ItemRecord* item_by_index(int index) {
    if (index < 1 || (size_t)index > kItemCount) return nullptr;
    return &items()[index - 1];
}

const char* const kClassName[kClassCount] = {
    "Siegurd", "Eljot", "Ihl Kazar", "Mask Bearer", "Dragon", "Earth", "Fire",
    "Water", "Air", "Angel", "Woodkeeper", "Phantomfighter", "Warrior",
    "Werewolf",
};

bool item_usable_by_class(int index, int prof) {
    const ItemRecord* it = item_by_index(index);
    if (!it || prof < 0 || (size_t)prof >= kProfessionCount) return false;
    return (it->f6 & (1 << prof)) != 0;
}

uint8_t item_use_index(int item_index) {
    const ItemRecord* it = item_by_index(item_index);
    return it ? it->spell : 0;
}

// The use routine (image 0x0E59E) takes the item's spell index, looks the spell
// up as `es:[spell*0x24 + 0x2712]` and reads its first parameter byte, which is
// `spells()[spell-1].data[0]` (see the note in char.h).
uint8_t item_use_flags(int item_index) {
    const ItemRecord* it = item_by_index(item_index);
    if (!it || it->spell == 0) return 0;
    const SpellRecord* sp = spell_by_index(it->spell);
    return sp ? sp->data[0] : 0;
}

// The handler at 0x0E59E takes the "cast" path only when the spell's first
// parameter byte has one of bits 0/1/2 set (it tests `test al,3` then
// `test al,4`); otherwise it prints "is not useful in this situation." (string
// ds:0x5C52, loaded at image 0x0E1A9).  The routine separately clears that
// verdict for **potions** (item class 9, tested at image 0x0E5C5/0x0E618), so a
// potion is always usable regardless of its spell's parameter byte.
//
// A `spell == 0` non-potion reaches neither branch, so the engine's verdict
// word is never written and the test at 0x0E5D5 reads uninitialised stack.  But
// live play settles the outcome: using an ordinary item prints "is not useful
// in this situation.", i.e. the engine treats it as not usable.  We return false
// to match (see docs/notes-b.md; the G-003 watchlist item is resolved).
bool item_is_usable(int item_index) {
    const ItemRecord* it = item_by_index(item_index);
    if (!it) return false;
    if (it->item_class == 9) return true;  // potions bypass the gate
    return (item_use_flags(item_index) & 0x07) != 0;
}

const SpellRecord* item_spell(int item_index) {
    const ItemRecord* it = item_by_index(item_index);
    if (!it || it->spell == 0) return nullptr;
    return spell_by_index(it->spell);
}

void dump_item_use(FILE* out) {
    for (size_t i = 1; i <= kItemCount; i++) {
        const ItemRecord* it = item_by_index((int)i);
        if (!it) break;
        std::fprintf(out, "  %3zu %-18s spell=%-3u gate=0x%02x%s\n", i,
                     it->name.c_str(), it->spell, item_use_flags((int)i),
                     item_is_usable((int)i) ? "  [usable]" : "");
    }
}

const char* item_effect_name(uint8_t effect) {
    switch (effect) {
        case 0: return "none";
        case 1: return "enchant weapon";
        case 2: return "enchant armour";
        case 3: return "poison I";
        case 4: return "gunpowder";
        case 5: return "crystal ball";
        case 7: return "tool";
        case 8: return "antimagic";
        case 9: return "catchpole hilt";
        case 10: return "healing";
        case 11: return "inquisitor";
        case 12: return "blade of power";
        case 13: return "special weapon";
        case 14: return "book half";
        case 15: return "quest jewel";
        case 16: return "scroll/ball";
        case 17: return "death mask";
        case 18: return "sarcophagus";
        case 19: return "protection scroll";
        case 20: return "holy water";
        case 21: return "dragon armour";
        case 23: return "poison II";
        case 24: return "healer staff";
        case 27: return "scroll of power";
        case 28: return "protecting ring";
        case 29: return "ring of shelter";
        case 30: return "antidote I";
        case 31: return "antidote II";
        case 32: return "spade";
        case 33: return "artifact weapon";
        case 34: return "lantern";
        case 35: return "torch";
        case 36: return "talisman";
        case 37: return "climbing gear";
        case 38: return "strength/food";
        case 39: return "map";
        case 40: return "raw material";
        default: return "none";
    }
}

std::vector<size_t> items_with_effect(uint8_t effect) {
    std::vector<size_t> out;
    const auto& v = items();
    for (size_t i = 0; i < v.size(); i++)
        if (v[i].effect == effect) out.push_back(i);
    return out;
}

void dump_items(std::FILE* out) {
    const auto& v = items();
    for (size_t i = 0; i < v.size(); i++) {
        const ItemRecord& r = v[i];
        std::fprintf(out,
                     "[%3zu] %-18s class=%u eff=%u (%-16s) price=%d wt=%d "
                     "dmg=%u chg=%u f0=%d f3=%d f4=%d f5=%d f6=%d\n",
                     i, r.name.c_str(), r.item_class, r.effect,
                     item_effect_name(r.effect), r.price, r.weight, r.damage,
                     r.charges, r.f0, r.f3, r.f4, r.f5, r.f6);
    }
}

const std::vector<SpellRecord>& spells() {
    static const std::vector<SpellRecord> table = [] {
        std::vector<SpellRecord> v;
        v.reserve(kSpellCount);
        for (size_t i = 0; i < kSpellCount; i++) {
            SpellRecord r;
            r.name = kSpellNames[i];
            std::memcpy(r.data, kSpellData[i], sizeof(r.data));
            v.push_back(std::move(r));
        }
        return v;
    }();
    return table;
}

const SpellRecord* spell_by_index(int index) {
    if (index < 1 || (size_t)index > kSpellCount) return nullptr;
    return &spells()[index - 1];
}

uint8_t magic_table_value(uint8_t profession, uint8_t level) {
    if (profession >= kMagicProfCount || level >= kMagicLevels) return 0;
    return kMagicTable[profession][level];
}

const char* spell_effect_name(uint8_t code) {
    switch (code) {
        case 0x00: return "damage";
        case 0x02: return "light";
        case 0x03: return "missile";
        case 0x04: return "paralyse";
        case 0x05: return "entangle";
        case 0x06: return "confuse";
        case 0x07: return "haste";
        case 0x08: return "mind";
        case 0x09: return "detect";
        case 0x0a: return "slow";
        case 0x0b: return "feeblemind";
        case 0x0c: return "fear";
        case 0x0d: return "alter";
        case 0x0e: return "summon elemental";
        case 0x0f: return "healing";
        case 0x10: return "petrify";
        case 0x11: return "turn undead";
        case 0x12: return "silence";
        case 0x13: return "cure poison";
        case 0x14: return "remove curse";
        case 0x15: return "holy";
        case 0x16: return "summon warrior";
        case 0x17: return "earthquake";
        case 0x19: return "protection";
        case 0x23: return "animal friendship";
        case 0x24: return "friendship";
        case 0x25: return "summon woodkeeper";
        case 0x27: return "critical";
        case 0x28: return "colour spray";
        case 0x29: return "polymorph";
        case 0x2b: return "illusion";
        case 0x2c: return "phantom";
        case 0x2d: return "cure blindness";
        case 0x2e: return "cure poison";
        case 0x2f: return "refresh";
        case 0x30: return "cure illness";
        case 0x31: return "shelter";
        case 0x33: return "restoration";
        case 0x34: return "magic shield";
        case 0x36: return "open locks";
        case 0x37: return "map";
        case 0x38: return "disarm traps";
        case 0x39: return "permanent light";
        case 0x3a: return "invisibility";
        case 0x3d: return "disrupt";
        case 0x3e: return "enchant";
        case 0x3f: return "titan's fist";
        default: return "special";
    }
}

std::vector<size_t> spells_with_effect(uint8_t code) {
    std::vector<size_t> out;
    const auto& v = spells();
    for (size_t i = 0; i < v.size(); i++)
        if (v[i].data[kSpellEffectByte] == code) out.push_back(i);
    return out;
}

void dump_spells(std::FILE* out) {
    const auto& v = spells();
    for (size_t i = 0; i < v.size(); i++) {
        std::fprintf(out, "[%3zu] %-22s eff=%02x (%-16s)", i, v[i].name.c_str(),
                     v[i].data[kSpellEffectByte],
                     spell_effect_name(v[i].data[kSpellEffectByte]));
        for (uint8_t b : v[i].data) std::fprintf(out, " %02x", b);
        std::fputc('\n', out);
    }
}

namespace {
std::vector<std::string> to_vec(const char* const* a, size_t n) {
    return std::vector<std::string>(a, a + n);
}
}  // namespace

std::vector<std::string> sex_names() { return to_vec(kSexNames, kSexCount); }
std::vector<std::string> race_names() { return to_vec(kRaceNames, kRaceCount); }
std::vector<std::string> profession_names() {
    return to_vec(kProfessionNames, kProfessionCount);
}
std::vector<std::string> alignment_names() {
    return to_vec(kAlignmentNames, kAlignmentCount);
}
std::vector<std::string> skill_names() {
    return to_vec(kSkillNames, kSkillCount);
}
std::vector<std::string> language_names() {
    return to_vec(kLanguageNames, kLanguageCount);
}
std::vector<std::string> status_short_names() {
    return to_vec(kStatusShortNames, kStatusCount);
}
std::vector<std::string> status_long_names() {
    return to_vec(kStatusLongNames, kStatusCount);
}
std::vector<std::string> gender_profession_names() {
    return to_vec(kGenderProfessionNames, kGenderProfessionCount);
}
std::vector<std::string> race_adjective_names() {
    return to_vec(kRaceAdjectiveNames, kRaceAdjectiveCount);
}
std::vector<std::string> alignment_lower_names() {
    return to_vec(kAlignmentLowerNames, kAlignmentLowerCount);
}
std::vector<std::string> profession_padded_names() {
    return to_vec(kProfessionPaddedNames, kProfessionDisplayCount);
}
std::vector<std::string> action_names() {
    return to_vec(kActionNames, kActionCount);
}
std::vector<std::string> morale_names() {
    return to_vec(kMoraleNames, kMoraleCount);
}
std::vector<std::string> combat_abbrev_names() {
    return to_vec(kCombatAbbrevNames, kCombatAbbrevCount);
}
std::vector<std::string> scenario_names() {
    return to_vec(kScenarioNames, kScenarioCount);
}
std::vector<std::string> facing_names() {
    return to_vec(kFacingNames, kFacingCount);
}
std::vector<std::string> time_of_day_names() {
    return to_vec(kTimeOfDayNames, kTimeOfDayCount);
}

std::vector<std::string> combat_intro_names() {
    return to_vec(kCombatIntroNames, kCombatIntroCount);
}

std::vector<std::string> combat_action_names() {
    return to_vec(kCombatActionNames, kCombatActionCount);
}

std::vector<std::string> combat_prompt_names() {
    return to_vec(kCombatPrompt, kCombatPromptCount);
}

std::vector<std::string> combat_round_header() {
    return to_vec(kCombatRoundHeader, kCombatRoundHeaderCount);
}

std::vector<std::string> combat_withdraw_header() {
    return to_vec(kCombatWithdrawHeader, kCombatWithdrawHeaderCount);
}

std::vector<std::string> combat_config_action_names() {
    return to_vec(kCombatConfigActionNames, kCombatConfigActionCount);
}

std::vector<std::string> combat_config_rank_names() {
    return to_vec(kCombatConfigRankNames, kCombatConfigRankCount);
}

std::vector<std::string> combat_continue_names() {
    return to_vec(kCombatContinueNames, kCombatContinueCount);
}

std::vector<std::string> combat_aim_names() {
    return to_vec(kCombatAimNames, kCombatAimCount);
}

std::string combat_group_line(uint32_t count, const std::string& plural,
                              uint32_t rank) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%lu %s (Rnk %lu)",
                  (unsigned long)count, plural.c_str(), (unsigned long)rank);
    return buf;
}

std::vector<std::string> monster_names(const std::string& dir, int scenario) {
    std::vector<std::string> out;
    if (scenario < 0 || scenario >= kMonsterScenarioCount) return out;
    char path[64];
    std::snprintf(path, sizeof(path), "%s/SYST/NSC0%d", dir.c_str(), scenario);
    for (const SnsRecord& r : load_nsc(path)) out.push_back(r.singular);
    return out;
}

std::vector<SnsRecord> monster_roster(const std::string& dir, int scenario) {
    if (scenario < 0 || scenario >= kMonsterScenarioCount) return {};
    char path[64];
    std::snprintf(path, sizeof(path), "%s/SYST/NSC0%d", dir.c_str(), scenario);
    return load_nsc(path);
}

CharSeed sns_char_seed(const SnsRecord& rec) {
    CharSeed s;
    // Grounded in the raw image at file offset 0x22F80..0x231A0 (the listing's
    // 1DA8E region).  Each field is `stats[off - 38]`.
    s.c14 = rec.stats[kSnsField3a - 38];   // +0x14 word
    s.c18 = rec.stats[kSnsClass4c - 38];
    s.c1c = rec.stats[kSnsBody2e - 38];
    s.c1d = (uint8_t)(100 - rec.stats[kSnsBody2d - 38]);
    s.c1e = (uint8_t)(100 - rec.stats[kSnsBody2d - 38]);
    s.c56 = rec.stats[kSnsSpeed36 - 38];
    s.hp_seed = rec.stats[kSnsHp2c - 38];
    s.c7a_index = rec.stats[kSnsIndex2a - 38];
    for (int n = 0; n < 4; ++n) {
        s.c142[n] = rec.stats[kSnsSkill3b + 2 * n - 38];
        s.c143[n] = rec.stats[kSnsSkill3c + 2 * n - 38];
    }
    return s;
}

int CharSheet::hp_percent() const {
    if (hp_max == 0) return 0;
    return 100 * hp_cur / hp_max;   // integer division, as the sheet builder does
}

std::string CharSheet::profession() const {
    auto names = profession_names();
    if (prof1b < names.size()) return names[prof1b];
    return {};
}

uint8_t CharSheet::magic_max() const {
    uint8_t base = magic_table_value(prof1b, level17);
    if (base == 0) return 0;
    if (wis48 > 15) base++;
    if (int47 > 15) base += 2;
    return base;
}

// --- party-status display row (builder at image offset 0x2D436) ------------
uint8_t CharSheet::hp_pct() const {
    if (hp_max == 0) return 0;
    return (uint8_t)(100 * hp_cur / hp_max);
}

uint8_t CharSheet::hp_mod() const {
    return (uint8_t)((hp_cur >> 3) + (flag6a ? 50 : 0));
}

uint8_t CharSheet::attr_a() const {
    uint8_t v = (uint8_t)(100 - (mod5b + stat1e));   // neg al after sub 0x64
    if (flag0f == 0) v = (uint8_t)(v - 10);
    return v;
}

uint8_t CharSheet::attr_b() const {
    return (uint8_t)(mod5c + base1c);
}

uint8_t CharSheet::attr_c() const {
    return (uint8_t)(mod5f + level17);
}

PartyRow party_row(const CharSheet& c, uint8_t index) {
    PartyRow r;
    r.index  = index;
    r.hp_pct = c.hp_pct();
    r.body24 = c.body24;
    r.hp_mod = c.hp_mod();
    r.attr_a = c.attr_a();
    r.attr_b = c.attr_b();
    r.attr_c = c.attr_c();
    r.hp_cur = (uint16_t)c.hp_cur;
    return r;
}

std::vector<PartyRow> party_status(const SaveGame& sv) {
    std::vector<PartyRow> rows;
    uint8_t idx = 0;
    for (const auto& sc : sv.chars) {
        if (sc.present != 1) continue;   // only slots the engine marks in use
        rows.push_back(party_row(decode_char(sc.raw.data()), idx++));
    }
    return rows;
}

std::string panel_stat_text(uint8_t stat1e) {
    return std::to_string((unsigned)stat1e) + "%";
}

uint8_t char_stat1d_from_dex(uint8_t dex) {
    if (dex < 16) return 10;
    return (uint8_t)(5 * dex - 65);   // 1BA40: 5*dex - 0x41
}

CharSheet decode_char(const uint8_t* raw) {
    CharSheet c;
    c.present = raw[kCharPresent];
    c.name = cstr(raw + kCharName, 14);
    c.flags10 = (int16_t)rd16(raw + kCharFlags10);
    c.counter12 = (int16_t)rd16(raw + kCharCounter12);
    c.level17 = raw[kCharLevel17];
    c.class18 = raw[kCharClass18];
    c.prof1b = raw[kCharProf1b];
    c.base1c = raw[kCharBase1c];
    c.stat1d = raw[kCharStat1d];
    c.stat1e = raw[kCharStat1e];
    c.hp_max = (int16_t)rd16(raw + kCharHpMax20);
    c.hp_cur = (int16_t)rd16(raw + kCharHpCur22);
    c.exp = rd32(raw + kCharExp6c);
    c.con44 = raw[kCharCon44];
    c.str45 = raw[kCharStr45];
    c.dex46 = raw[kCharDex46];
    c.int47 = raw[kCharInt47];
    c.wis48 = raw[kCharWis48];
    c.speed56 = raw[kCharSpeed56];
    c.body24 = raw[kCharBody24];
    c.mod5b = raw[kCharFlg5b];
    c.mod5c = raw[kCharFlg5c];
    c.mod5e = raw[kCharFlg5e];
    c.mod5f = raw[kCharFlg5f];
    c.flag6a = raw[kCharFlag6a];
    c.flag0f = raw[kCharFlg0f];
    c.mag_max68 = raw[kCharMagMax68];
    c.mag_cur69 = raw[kCharMagCur69];
    c.load25 = raw[kCharLoad25];
    c.inv_used = raw[kCharInvUsed6b];
    c.weight_max = rd16(raw + kCharWeight64);
    c.weight_cur = rd16(raw + kCharWeight66);
    c.gold = rd32(raw + kCharGold72);
    c.rations70 = raw[kCharRations70];
    for (size_t s = 0; s < kCharInvCount; ++s) {
        const uint8_t* p = raw + kCharInv82 + s * kCharInvStride;
        c.inv[s] = InvSlot{p[0], p[1], p[2], p[3]};
    }
    return c;
}

uint32_t carried_item_weight(const CharSheet& c) {
    uint32_t sum = 0;
    for (const InvSlot& s : c.inv) {
        const ItemRecord* it = item_by_index(s.item);
        if (it && it->weight > 0) sum += (uint32_t)it->weight;
    }
    return sum;
}

uint32_t weight_base(const CharSheet& c) {
    return 10u * c.rations70 + c.gold;
}

uint8_t defense_from_items(const CharSheet& c) {
    uint32_t total = c.stat1d;
    // The engine's armour routine (0x210E8) iterates the slots only up to the
    // used count +0x6b, so cap here too (a stray non-empty slot past the count
    // would otherwise leak into the AC).
    size_t limit = (size_t)c.inv_used <= kCharInvCount ? (size_t)c.inv_used
                                                       : kCharInvCount;
    for (size_t s = 0; s < limit; ++s) {
        const InvSlot& slot = c.inv[s];
        if (slot.item == 0 || slot.equipped == 0) continue;
        const ItemRecord* it = item_by_index(slot.item);
        if (!it) continue;
        // The armour routine reads the LOW byte of item +0x16 only
        // (`mov al, es:[bx+0xfd2]`, image 0x2116b); the high byte is the
        // initial quantity/charges, not part of the AC.
        uint8_t ac = (uint8_t)((uint16_t)it->f3 & 0xff);
        if (ac == 0) continue;
        uint32_t v = (uint32_t)slot.condition + 10;
        v -= v % 20;
        total += (uint32_t)ac * v / 100;
    }
    if (total > 0x5f) total = 0x5f;
    return (uint8_t)total;
}

namespace {

// The engine's item-scan helper (image 0x21D2): the first inventory slot at or
// after `from` whose item's class byte equals `cls`; -1 when none.
int find_slot_of_class(const CharSheet& c, int cls, int from) {
    for (int s = from; s < (int)kCharInvCount; ++s) {
        if (c.inv[s].item == 0) continue;
        const ItemRecord* it = item_by_index(c.inv[s].item);
        if (it && it->item_class == cls) return s;
    }
    return -1;
}

}  // namespace

WeaponSlots weapon_slots(const CharSheet& c) {
    WeaponSlots w;
    w.slot[0] = find_slot_of_class(c, 1, 0);            // 0x1398B, class 1
    if (w.slot[0] < 0) w.slot[0] = find_slot_of_class(c, 5, 0);  // 0x13A15
    if (w.slot[0] < 0) return w;
    w.slot[1] = find_slot_of_class(c, 1, w.slot[0] + 1);         // 0x139AA
    for (int h = 0; h < 2; ++h) {
        if (w.slot[h] < 0) continue;
        const ItemRecord* it = item_by_index(c.inv[w.slot[h]].item);
        w.damage[h] = it ? it->damage : 0;
        w.f0[h] = it ? it->f0 : -1;
    }
    return w;
}

PartyRow defender_row(const SnsRecord& rec, int group, int hp, int hp_max) {
    PartyRow r;
    r.index = 0;
    r.hp_pct = rec.stats[0x2c - 0x26] ? (uint8_t)(100 * hp / (8 * rec.stats[0x2c - 0x26]))
                                      : 0;
    r.body24 = (uint8_t)(group + 1);
    // +0x06: the encounter builder (0x27A1E) writes 0 or 0x32, but a live
    // round rebuilds the rows: it zeroes the byte (runtime 313F:6870) and then
    // subtracts 10 * (4 - rec[+0x29]) (313F:68D5..0x6902).  The Mercenary
    // capture reads 0xEC = -(10 * (4 - 2)), which pins the formula.
    {
        const uint8_t step = (uint8_t)(4u - (uint32_t)rec.stats[0x29 - 0x26]);
        r.hp_mod = (uint8_t)(0u - (uint8_t)(step * 10u));
    }
    r.attr_a = rec.stats[0x2d - 0x26];
    r.attr_b = rec.stats[0x2e - 0x26];
    r.attr_c = rec.stats[0x30 - 0x26];
    r.hp_cur = (uint16_t)hp;
    (void)hp_max;
    return r;
}

}  // namespace lofport
