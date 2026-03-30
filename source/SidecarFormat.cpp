#include "SidecarFormat.h"

namespace SidecarFormat {

// ------------------------------------------------------------
// Format constants
// ------------------------------------------------------------
static const unsigned int kMagic          = 0x31575447; // 'GTW1' little-endian
static const unsigned int kLegacyVersion  = 1;
static const unsigned int kChunkedVersion = 2;
static const unsigned int kTag_OWNR       = 0x524E574F; // 'OWNR' little-endian

// ------------------------------------------------------------
// Binary helpers
// ------------------------------------------------------------
static void PushU32(std::vector<unsigned char>& out, unsigned int v) {
    out.push_back((unsigned char)(v & 0xFF));
    out.push_back((unsigned char)((v >>  8) & 0xFF));
    out.push_back((unsigned char)((v >> 16) & 0xFF));
    out.push_back((unsigned char)((v >> 24) & 0xFF));
}

static bool ReadU32(const std::vector<unsigned char>& b, size_t& i, unsigned int& v) {
    if (i + 4 > b.size()) return false;
    v = (unsigned int)b[i]
      | ((unsigned int)b[i+1] <<  8)
      | ((unsigned int)b[i+2] << 16)
      | ((unsigned int)b[i+3] << 24);
    i += 4;
    return true;
}

static void PushU16(std::vector<unsigned char>& out, unsigned short v) {
    out.push_back((unsigned char)(v & 0xFF));
    out.push_back((unsigned char)((v >> 8) & 0xFF));
}

static bool ReadU16(const std::vector<unsigned char>& b, size_t& i, unsigned short& v) {
    if (i + 2 > b.size()) return false;
    v = (unsigned short)b[i] | ((unsigned short)b[i+1] << 8);
    i += 2;
    return true;
}

// ------------------------------------------------------------
// OWNR payload helpers
// ------------------------------------------------------------
static std::vector<unsigned char> BuildOwnrPayload(const std::vector<Entry>& entries) {
    std::vector<unsigned char> out;
    out.reserve(4 + entries.size() * 16);
    PushU32(out, (unsigned int)entries.size());
    for (const auto& e : entries) {
        const auto len = (unsigned short)e.id.size();
        PushU16(out, len);
        out.insert(out.end(), e.id.begin(), e.id.end());
        PushU32(out, (unsigned int)(e.ownerGang));
    }
    return out;
}

static bool ParseOwnrPayload(
    const std::vector<unsigned char>& bytes,
    size_t& i,
    std::vector<Entry>& outEntries,
    std::string& outErr)
{
    outEntries.clear();

    unsigned int count = 0;
    if (!ReadU32(bytes, i, count)) { outErr = "OWNR: missing count"; return false; }
    if (count > 4096)              { outErr = "OWNR: count too large"; return false; }

    outEntries.reserve(count);
    for (unsigned int n = 0; n < count; ++n) {
        unsigned short len = 0;
        if (!ReadU16(bytes, i, len))      { outErr = "OWNR: missing idLen"; return false; }
        if (i + len > bytes.size())       { outErr = "OWNR: id bytes out of range"; return false; }

        Entry e;
        e.id.assign((const char*)&bytes[i], (size_t)len);
        i += len;

        unsigned int ownerU = 0;
        if (!ReadU32(bytes, i, ownerU))   { outErr = "OWNR: missing owner"; return false; }
        e.ownerGang = (int)ownerU;
        outEntries.push_back(e);
    }

    if (outEntries.size() != count) {
        outErr = "OWNR: parsed count mismatch";
        return false;
    }
    return true;
}

// ------------------------------------------------------------
// Public API
// ------------------------------------------------------------
std::vector<unsigned char> Serialize(const std::vector<Entry>& entries) {
    auto ownr = BuildOwnrPayload(entries);

    std::vector<unsigned char> out;
    out.reserve(12 + 8 + ownr.size());

    PushU32(out, kMagic);
    PushU32(out, kChunkedVersion);
    PushU32(out, 1u); // chunkCount

    PushU32(out, kTag_OWNR);
    PushU32(out, (unsigned int)ownr.size());
    out.insert(out.end(), ownr.begin(), ownr.end());

    return out;
}

ParseResult Deserialize(const std::vector<unsigned char>& bytes) {
    ParseResult result;
    size_t i = 0;

    unsigned int magic = 0, ver = 0;
    if (!ReadU32(bytes, i, magic) || !ReadU32(bytes, i, ver)) {
        result.error = "corrupt header";
        return result;
    }
    if (magic != kMagic) {
        result.error = "bad magic";
        return result;
    }

    // -------------------------
    // v1 legacy format
    // -------------------------
    if (ver == kLegacyVersion) {
        unsigned int count = 0;
        if (!ReadU32(bytes, i, count) || count > 4096) {
            result.error = "v1: bad count";
            return result;
        }

        result.entries.reserve(count);
        for (unsigned int n = 0; n < count; ++n) {
            unsigned short len = 0;
            if (!ReadU16(bytes, i, len) || i + len > bytes.size()) {
                result.error = "v1: corrupt entry";
                return result;
            }
            Entry e;
            e.id.assign((const char*)&bytes[i], (size_t)len);
            i += len;

            unsigned int ownerU = 0;
            if (!ReadU32(bytes, i, ownerU)) {
                result.error = "v1: missing owner";
                return result;
            }
            e.ownerGang = (int)ownerU;
            result.entries.push_back(e);
        }

        if (result.entries.size() != count) {
            result.error = "v1: count mismatch";
            return result;
        }
        result.ok = true;
        return result;
    }

    // -------------------------
    // v2+ chunked format
    // -------------------------
    if (ver >= kChunkedVersion) {
        unsigned int chunkCount = 0;
        if (!ReadU32(bytes, i, chunkCount) || chunkCount > 64) {
            result.error = "v2: bad chunkCount";
            return result;
        }

        bool foundOwnr = false;
        for (unsigned int c = 0; c < chunkCount; ++c) {
            unsigned int tag = 0, len = 0;
            if (!ReadU32(bytes, i, tag) || !ReadU32(bytes, i, len)) {
                result.error = "v2: corrupt chunk header";
                return result;
            }
            if (i + len > bytes.size()) {
                result.error = "v2: chunk len out of range";
                return result;
            }

            if (tag == kTag_OWNR) {
                size_t pi = i;
                if (!ParseOwnrPayload(bytes, pi, result.entries, result.error)) {
                    return result;
                }
                foundOwnr = true;
            }
            // Unknown chunks are skipped for forward compatibility
            i += len;
        }

        if (!foundOwnr) {
            result.error = "v2: missing OWNR chunk";
            return result;
        }
        result.ok = true;
        return result;
    }

    result.error = "unknown version";
    return result;
}

} // namespace SidecarFormat
