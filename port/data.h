// Readers for the Legend of Faerghail data files (see AGENTS.md for formats).
#ifndef LOF_PORT_DATA_H
#define LOF_PORT_DATA_H

#include <cstdint>
#include <string>
#include <vector>

namespace lofport {

// One 78-byte entry from SYST/SNSC or SYST/NSCnn: a singular and a plural
// NUL-padded name (19 bytes each) followed by a 40-byte statistics block.
struct SnsRecord {
    std::string singular;
    std::string plural;
    uint8_t stats[40] = {0};
};

// SYST/SNSC: 14 class records.
std::vector<SnsRecord> load_snsc(const std::string& path);

// SYST/NSCnn: a count byte then that many records.
std::vector<SnsRecord> load_nsc(const std::string& path);

// SYST/TXTn: a word-offset table followed by NUL-terminated strings.
std::vector<std::string> load_txt(const std::string& path);

}  // namespace lofport

#endif
