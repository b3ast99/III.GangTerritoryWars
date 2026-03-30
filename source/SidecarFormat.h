#pragma once
#include <vector>
#include <string>

// Pure binary serialization for the GTW1 territory sidecar format.
// No game engine dependencies — safe to include in unit test projects.

namespace SidecarFormat {

struct Entry {
    std::string id;
    int ownerGang = -1;
};

struct ParseResult {
    bool ok = false;
    std::string error;
    std::vector<Entry> entries;
};

// Build a v2 chunked GTW1 binary blob from ownership entries.
std::vector<unsigned char> Serialize(const std::vector<Entry>& entries);

// Parse a GTW1 binary blob. Accepts v1 legacy and v2+ chunked formats.
// Returns ok=false with a descriptive error on any failure.
ParseResult Deserialize(const std::vector<unsigned char>& bytes);

} // namespace SidecarFormat
