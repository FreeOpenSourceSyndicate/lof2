#include "data.h"

#include <cstdio>
#include <cstring>

namespace lofport {

static std::vector<uint8_t> read_file(const std::string& path) {
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

static std::string cname(const uint8_t* p, size_t n) {
    size_t l = 0;
    while (l < n && p[l]) l++;
    return std::string((const char*)p, l);
}

static SnsRecord parse_record(const uint8_t* r) {
    SnsRecord s;
    s.singular = cname(r, 19);
    s.plural = cname(r + 19, 19);
    std::memcpy(s.stats, r + 38, 40);
    return s;
}

std::vector<SnsRecord> load_snsc(const std::string& path) {
    std::vector<uint8_t> d = read_file(path);
    std::vector<SnsRecord> out;
    for (size_t o = 0; o + 78 <= d.size(); o += 78)
        out.push_back(parse_record(&d[o]));
    return out;
}

std::vector<SnsRecord> load_nsc(const std::string& path) {
    std::vector<uint8_t> d = read_file(path);
    std::vector<SnsRecord> out;
    if (d.empty()) return out;
    size_t count = d[0];
    for (size_t i = 0; i < count; i++) {
        size_t o = 1 + i * 78;
        if (o + 78 > d.size()) break;
        out.push_back(parse_record(&d[o]));
    }
    return out;
}

std::vector<std::string> load_txt(const std::string& path) {
    std::vector<uint8_t> d = read_file(path);
    std::vector<std::string> out;
    if (d.size() < 2) return out;
    // The offset table runs up to the first string body; walk it while each
    // offset is a plausible word value pointing inside the file.
    size_t first = d[0] | (d[1] << 8);
    if (first < 2 || first > d.size()) first = d.size();
    for (size_t i = 0; i + 2 <= first; i += 2) {
        size_t o = d[i] | (d[i + 1] << 8);
        if (o >= d.size()) break;
        size_t e = o;
        while (e < d.size() && d[e]) e++;
        out.push_back(std::string((const char*)&d[o], e - o));
    }
    return out;
}

}  // namespace lofport
