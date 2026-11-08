#include "char.h"

#include <cstring>

namespace lofport {

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
    size_t n = (d.size() - kSaveHeaderSize) / 410;
    for (size_t i = 0; i < n; i++) {
        const uint8_t* r = d.data() + kSaveHeaderSize + i * 410;
        SaveChar c;
        std::memcpy(c.raw.data(), r, 410);
        c.present = r[0];
        std::string nm;
        for (int k = 1; k < 15 && r[k]; k++) nm.push_back((char)r[k]);
        c.name = std::move(nm);
        g.chars.push_back(std::move(c));
    }
    size_t end = kSaveHeaderSize + n * 410;
    g.trailer.assign(d.begin() + end, d.end());
    return g;
}

ShopData load_shop(const std::string& path) {
    ShopData s;
    std::vector<uint8_t> d = read_file(path);
    if (d.empty()) return s;
    // Byte 0 is a zero header; bytes 1..45 are signed adjustments.
    size_t n = d.size() > 46 ? 45 : (d.size() > 1 ? d.size() - 1 : 0);
    s.adjustments.reserve(n);
    for (size_t i = 0; i < n; i++) s.adjustments.push_back((int8_t)d[1 + i]);
    // The file ends with a 16-bit value (0x1606 in this release) followed by
    // two zero bytes.
    if (d.size() >= 4) s.trailing = rd16(d.data() + d.size() - 4);
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
                     "  [%3zu] type=%-3u (%-11s) idx=%02x arg4c=%u arg4e=%d cnt=%u\n",
                     i, (unsigned)r.type, spec_type_name(r.type), r.index_c9,
                     r.arg_4c, r.arg_4e, r.counter);
        if (!r.text_a.empty()) std::fprintf(out, "        a: %s\n", r.text_a.c_str());
        if (!r.text_b.empty()) std::fprintf(out, "        b: %s\n", r.text_b.c_str());
    }
}

}  // namespace lofport
