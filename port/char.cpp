#include "char.h"

#include <cstdio>
#include <cstring>

#include "data.h"

namespace lofport {

#include "char_items.inc"
#include "char_spells.inc"
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

// Read a NUL-terminated string from `p`, at most `limit` bytes.
std::string cstr(const uint8_t* p, size_t limit) {
    size_t l = 0;
    while (l < limit && p[l]) l++;
    return std::string((const char*)p, l);
}

}  // namespace

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

        rec.text_a = cstr(r + 0x0e, 0x4a - 0x0e);
        rec.text_b = cstr(r + 0x50, 0xc6 - 0x50);

        rec.flag_4b = r[0x4b];
        rec.arg_4c = rd16(r + 0x4c);
        rec.arg_4e = (int16_t)rd16(r + 0x4e);
        rec.index_c9 = r[0xc9];
        rec.arg_da = rd16(r + 0xda);
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
    size_t n = (d.size() - kSaveHeaderSize) / 410;
    for (size_t i = 0; i < n; i++) {
        const uint8_t* r = d.data() + kSaveHeaderSize + i * 410;
        SaveChar c;
        std::memcpy(c.raw.data(), r, 410);
        c.present = r[0];
        c.profession = r[0x1b];
        std::string nm;
        for (int k = 1; k < 15 && r[k]; k++) nm.push_back((char)r[k]);
        c.name = std::move(nm);
        g.chars.push_back(std::move(c));
    }
    size_t end = kSaveHeaderSize + n * 410;
    g.trailer.assign(d.begin() + end, d.end());
    return g;
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
    // Byte 0 is a header (zero in this release); the next 185 bytes are one
    // signed adjustment per item, parallel to the item table order (the engine
    // reads 186 then 4).  The file ends with a 16-bit trailer word.
    if (d.size() < 1 + kItemCount) return s;
    for (size_t i = 0; i < kItemCount; i++) s.adjustments[i] = (int8_t)d[1 + i];
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
            r.f1 = kItemFields[i][1];
            r.f2 = kItemFields[i][2];
            r.f3 = kItemFields[i][3];
            r.f4 = kItemFields[i][4];
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

bool item_usable_by_class(int index, int cls) {
    const ItemRecord* it = item_by_index(index);
    if (!it || cls < 0 || (size_t)cls >= kClassCount) return false;
    return (it->f6 & (1 << cls)) != 0;
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
                     "[%3zu] %-18s class=%u eff=%u (%-16s) f0..6=%d %d %d %d %d %d "
                     "%d\n",
                     i, r.name.c_str(), r.item_class, r.effect,
                     item_effect_name(r.effect), r.f0, r.f1, r.f2, r.f3, r.f4,
                     r.f5, r.f6);
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

std::vector<std::string> monster_names(const std::string& dir, int scenario) {
    std::vector<std::string> out;
    if (scenario < 0 || scenario >= kMonsterScenarioCount) return out;
    char path[64];
    std::snprintf(path, sizeof(path), "%s/SYST/NSC0%d", dir.c_str(), scenario);
    for (const SnsRecord& r : load_nsc(path)) out.push_back(r.singular);
    return out;
}

}  // namespace lofport
