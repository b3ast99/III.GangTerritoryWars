#include "SaveSlotParser.h"
#include <cstring>
#include <cctype>

#ifdef _WIN32
#include <string.h>  // _stricmp
#else
#include <strings.h> // strcasecmp
#define _stricmp strcasecmp
#endif

namespace SaveSlotParser {

bool TryParseSaveSlot(const char* filePath, int& outSlot) {
    if (!filePath) return false;

    const char* key = "GTA3sf";
    const char* p = std::strstr(filePath, key);
    if (!p) return false;
    p += std::strlen(key);

    if (!std::isdigit((unsigned char)*p)) return false;

    int slot = 0;
    while (std::isdigit((unsigned char)*p)) {
        slot = slot * 10 + (*p - '0');
        ++p;
    }

    if (slot < 1 || slot > 8) return false;

    // Must end with exactly ".b" (not ".bak" etc.)
    const char* ext = std::strrchr(filePath, '.');
    if (!ext || _stricmp(ext, ".b") != 0) return false;

    outSlot = slot;
    return true;
}

bool IsReadMode(const char* mode) {
    if (!mode) return false;
    const bool hasR = (std::strchr(mode, 'r') != nullptr);
    const bool hasW = (std::strchr(mode, 'w') != nullptr);
    const bool hasA = (std::strchr(mode, 'a') != nullptr);
    return hasR && !hasW && !hasA;
}

bool IsWriteMode(const char* mode) {
    if (!mode) return false;
    return (std::strchr(mode, 'w') != nullptr) || (std::strchr(mode, 'a') != nullptr);
}

} // namespace SaveSlotParser
