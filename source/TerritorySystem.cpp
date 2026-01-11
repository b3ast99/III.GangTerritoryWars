#include "TerritorySystem.h"
#include "TerritoryRadarRenderer.h"
#include "DebugLog.h"
#include "WaveManager.h"

#include "CRadar.h"
#include "CWorld.h"
#include "CPlayerPed.h"
#include "CMessages.h"
#include "CTimer.h"

#include <cstdio>
#include <algorithm>
#include <sys/stat.h>
#include <cctype>
#include <cstring>
#include <windows.h>

// ------------------------------------------------------------
// territories.txt format:
// id,minX,minY,maxX,maxY[,ownerGangCode[,underAttack[,defenseLevel]]]
//
// ownerGangCode is treated as DEFAULT owner (loaded into defaultOwnerGang)
// underAttack is ignored as default and treated runtime-only
// ------------------------------------------------------------

static float Dist2(float ax, float ay, float bx, float by) {
    const float dx = ax - bx;
    const float dy = ay - by;
    return dx * dx + dy * dy;
}

static bool IsAllDigits(const std::string& s)
{
    if (s.empty()) return false;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

static void StripNewline(char* s) {
    if (!s) return;
    for (char* p = s; *p; ++p) {
        if (*p == '\n' || *p == '\r') { *p = 0; break; }
    }
}

static char* LTrim(char* s) {
    while (*s && std::isspace((unsigned char)*s)) ++s;
    return s;
}

static void RTrim(char* s) {
    size_t n = std::strlen(s);
    while (n > 0 && std::isspace((unsigned char)s[n - 1])) {
        s[n - 1] = 0;
        --n;
    }
}

static const char* GetConfigPathRelativeToASI()
{
    static char path[MAX_PATH];
    static bool initialized = false;

    if (!initialized) {
        HMODULE hMod = nullptr;
        GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&GetConfigPathRelativeToASI,
            &hMod
        );

        char modulePath[MAX_PATH];
        GetModuleFileNameA(hMod, modulePath, MAX_PATH);

        char* lastSlash = strrchr(modulePath, '\\');
        if (lastSlash) {
            *(lastSlash + 1) = '\0';
        }

        size_t moduleLen = strlen(modulePath);
        size_t needed = moduleLen + strlen("territories.txt") + 1;
        if (needed <= sizeof(path)) {
            snprintf(path, sizeof(path), "%sterritories.txt", modulePath);
        } else {
            DebugLog::Write("Path too long, using current directory");
            strcpy_s(path, sizeof(path), "territories.txt");
        }

        DebugLog::Write("Territory config path: %s", path);
        initialized = true;
    }

    return path;
}

// ------------------------------------------------------------
// Static members
// ------------------------------------------------------------
std::vector<Territory> TerritorySystem::s_territories;
bool TerritorySystem::s_overlayEnabled = true;

unsigned int TerritorySystem::s_nextReloadPollMs = 0;
long long TerritorySystem::s_lastConfigStamp = -1;
unsigned int TerritorySystem::s_lastReloadFailToastMs = 0;

TerritorySystem::EditorState TerritorySystem::s_editor{};

const char* TerritorySystem::ConfigPath() {
    return GetConfigPathRelativeToASI();
}

void TerritorySystem::NormalizeRect(Territory& t) {
    if (t.minX > t.maxX) std::swap(t.minX, t.maxX);
    if (t.minY > t.maxY) std::swap(t.minY, t.maxY);
}

long long TerritorySystem::GetConfigStampOrNeg1() {
    struct stat s {};
    if (stat(ConfigPath(), &s) != 0) return -1;
    return (long long)s.st_mtime;
}

static bool ParseLineTerritory(const char* line, Territory& outT, std::string& outErr) {
    if (!line || !line[0]) { outErr = "Empty line"; return false; }

    char buf[512];
    std::snprintf(buf, sizeof(buf), "%s", line);

    char* tokens[8] = { 0 };
    int count = 0;

    char* p = buf;
    while (count < 8) {
        tokens[count++] = p;
        char* comma = std::strchr(p, ',');
        if (!comma) break;
        *comma = 0;
        p = comma + 1;
    }

    for (int i = 0; i < count; i++) {
        char* t = LTrim(tokens[i]);
        if (t != tokens[i]) std::memmove(tokens[i], t, std::strlen(t) + 1);
        RTrim(tokens[i]);
    }

    if (count < 5) {
        outErr = "Expected at least 5 comma-separated fields";
        return false;
    }

    Territory t{};
    t.id = tokens[0];
    if (t.id.empty()) { outErr = "Missing id"; return false; }

    if (!IsAllDigits(t.id)) {
        outErr = "Id must be numeric (e.g. 1001)";
        return false;
    }

    if (std::sscanf(tokens[1], "%f", &t.minX) != 1) { outErr = "Bad minX"; return false; }
    if (std::sscanf(tokens[2], "%f", &t.minY) != 1) { outErr = "Bad minY"; return false; }
    if (std::sscanf(tokens[3], "%f", &t.maxX) != 1) { outErr = "Bad maxX"; return false; }
    if (std::sscanf(tokens[4], "%f", &t.maxY) != 1) { outErr = "Bad maxY"; return false; }

    t.ownerGang = -1;
    t.defaultOwnerGang = -1;
    t.underAttack = false;
    t.defenseLevel = 1;

    if (count >= 6 && tokens[5] && tokens[5][0]) {
        int code = -1;
        if (std::sscanf(tokens[5], "%d", &code) != 1) { outErr = "Bad ownerGangCode"; return false; }
        t.ownerGang = code;
        t.defaultOwnerGang = code;
    } else {
        t.ownerGang = -1;
        t.defaultOwnerGang = -1;
    }

    // UnderAttack in file is ignored for defaults; always runtime-only
    // Defense level optional
    if (count >= 8 && tokens[7] && tokens[7][0]) {
        int dl = 1;
        if (std::sscanf(tokens[7], "%d", &dl) != 1) { outErr = "Bad defenseLevel"; return false; }
        if (dl < 0) dl = 0;
        if (dl > 2) dl = 2;
        t.defenseLevel = dl;
    }

    if (t.minX > t.maxX) std::swap(t.minX, t.maxX);
    if (t.minY > t.maxY) std::swap(t.minY, t.maxY);

    outT = t;
    return true;
}

bool TerritorySystem::LoadFromFile(std::vector<Territory>& out, std::string& outErr) {
    out.clear();

    FILE* f = std::fopen(ConfigPath(), "rb");
    if (!f) {
        outErr = "Could not open territories.txt";
        return false;
    }

    char line[512];
    int lineNo = 0;

    while (std::fgets(line, sizeof(line), f)) {
        lineNo++;

        StripNewline(line);
        char* p = LTrim(line);
        RTrim(p);

        if (!p[0]) continue;
        if (p[0] == '#') continue;

        Territory t{};
        std::string perr;
        if (!ParseLineTerritory(p, t, perr)) {
            char buf2[256];
            std::snprintf(buf2, sizeof(buf2), "Parse error line %d: %s", lineNo, perr.c_str());
            outErr = buf2;
            std::fclose(f);
            return false;
        }

        auto dup = std::find_if(out.begin(), out.end(), [&](const Territory& x) { return x.id == t.id; });
        if (dup != out.end()) {
            char buf2[256];
            std::snprintf(buf2, sizeof(buf2), "Duplicate id '%s' at line %d", t.id.c_str(), lineNo);
            outErr = buf2;
            std::fclose(f);
            return false;
        }

        out.push_back(t);
    }

    std::fclose(f);

    if (out.empty()) {
        outErr = "No territories loaded";
        return false;
    }

    return true;
}

// NOTE: SaveToFile is still used by the editor to modify rectangles/defaults.
static bool AtomicWriteTerritories(const char* finalPath, const char* tmpPath, const char* bakPath,
    const std::vector<Territory>& terrs, std::string& outErr)
{
    FILE* f = std::fopen(tmpPath, "wb");
    if (!f) { outErr = "Failed to open temp file for write"; return false; }

    std::fprintf(f, "# id,minX,minY,maxX,maxY,ownerGangCode,underAttack,defenseLevel\n");
    for (const auto& t : terrs) {
        std::fprintf(f, "%s,%.3f,%.3f,%.3f,%.3f,%d,%d,%d\n",
            t.id.c_str(), t.minX, t.minY, t.maxX, t.maxY,
            t.defaultOwnerGang, 0, t.defenseLevel);
    }

    std::fclose(f);

    std::remove(bakPath);
    std::rename(finalPath, bakPath);
    if (std::rename(tmpPath, finalPath) != 0) {
        outErr = "Failed to move temp file into place";
        std::remove(tmpPath);
        return false;
    }
    return true;
}

bool TerritorySystem::SaveToFile(const std::vector<Territory>& terrs, std::string& outErr) {
    std::vector<Territory> sorted = terrs;
    std::sort(sorted.begin(), sorted.end(),
        [](const Territory& a, const Territory& b) { return a.id < b.id; });

    const char* finalPath = ConfigPath();

    char tmpPath[MAX_PATH];
    char bakPath[MAX_PATH];
    std::snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", finalPath);
    std::snprintf(bakPath, sizeof(bakPath), "%s.bak", finalPath);

    return AtomicWriteTerritories(finalPath, tmpPath, bakPath, sorted, outErr);
}

int TerritorySystem::ComputeNextId(const std::vector<Territory>& terrs) {
    int best = 1000;
    for (const auto& t : terrs) {
        int id = std::atoi(t.id.c_str());
        if (id > best) best = id;
    }
    return best + 1;
}

bool TerritorySystem::GetPlayerXY(float& outX, float& outY) {
    CPlayerPed* player = CWorld::Players[0].m_pPed;
    if (!player) return false;
    const CVector pos = player->GetPosition();
    outX = pos.x;
    outY = pos.y;
    return true;
}

void TerritorySystem::TryReloadNow(bool showToastOnFail) {
    DebugLog::Write("TerritorySystem::TryReloadNow called (warActive=%d)", (int)WaveManager::IsWarActive());
    // Snapshot runtime ownership BEFORE we reload the file.
    // This is the key: hot reload should keep the persisted/runtime ownership in memory,
    // not revert ownership back to defaults from territories.txt.
    std::vector<OwnershipEntry> prevOwnership;
    GetOwnershipState(prevOwnership);

    std::vector<Territory> next;
    std::string err;

    if (!LoadFromFile(next, err)) {
        const unsigned int now = CTimer::m_snTimeInMilliseconds;
        if (showToastOnFail && (now - s_lastReloadFailToastMs) > 2000) {
            s_lastReloadFailToastMs = now;
            DebugLog::Write("TerritorySystem: Reload failed: %s", err.c_str());
        }
        DebugLog::Write("TerritorySystem::TryReloadNow: LoadFromFile FAILED -> returning early (warActive=%d)",
            (int)WaveManager::IsWarActive());
        return;
    }

    if (WaveManager::IsWarActive()) {
        DebugLog::Write("TerritorySystem: hot reload during war -> cancel war");
        WaveManager::CancelWar();
    }


    // Swap in the new geometry/defaults from file…
    s_territories.swap(next);

    // …then re-apply runtime ownership from memory (sidecar state).
    // Any IDs not found in prevOwnership will remain whatever the file says (defaults).
    ApplyOwnershipState(prevOwnership);

    // Runtime-only flags should stay runtime-only.
    ClearAllWarsAndTransientState();

    s_editor.nextId = ComputeNextId(s_territories);

    DebugLog::Write("TerritorySystem: Reloaded %d territories (ownership preserved)", (int)s_territories.size());
    s_lastConfigStamp = GetConfigStampOrNeg1();
}


void TerritorySystem::ForceReloadNow() {
    TryReloadNow(true);
}

void TerritorySystem::HotReloadTick(unsigned int nowMs) {
    if (nowMs < s_nextReloadPollMs) return;
    s_nextReloadPollMs = nowMs + 1000;

    const long long stamp = GetConfigStampOrNeg1();
    if (stamp < 0) return;

    if (s_lastConfigStamp < 0) {
        s_lastConfigStamp = stamp;
        return;
    }

    if (stamp != s_lastConfigStamp) {
        TryReloadNow(true);
    }
}

void TerritorySystem::Init() {
    s_territories.clear();
    s_overlayEnabled = true;

    s_nextReloadPollMs = 0;
    s_lastReloadFailToastMs = 0;

    TryReloadNow(true);

    s_editor.enabled = false;
    s_editor.hasA = false;
    s_editor.hasB = false;
    s_editor.defaultOwnerGang = -1;
}

void TerritorySystem::Shutdown() {
    s_territories.clear();
}

void TerritorySystem::Update() {
    const unsigned int now = CTimer::m_snTimeInMilliseconds;
    HotReloadTick(now);
}

void TerritorySystem::ToggleOverlay() {
    s_overlayEnabled = !s_overlayEnabled;
    DebugLog::Write(s_overlayEnabled ? "Territory overlay: ON" : "Territory overlay: OFF");
}

bool TerritorySystem::IsOverlayEnabled() {
    return s_overlayEnabled;
}

const Territory* TerritorySystem::GetTerritoryAtPoint(const CVector& pos) {
    for (const auto& t : s_territories) {
        if (t.ContainsPoint(pos)) return &t;
    }
    return nullptr;
}

const Territory* TerritorySystem::GetTerritoryAtPlayer() {
    CPlayerPed* player = CWorld::Players[0].m_pPed;
    if (!player) return nullptr;
    return GetTerritoryAtPoint(player->GetPosition());
}

bool TerritorySystem::HasRealTerritories() {
    return !s_territories.empty();
}

int TerritorySystem::GetPlayerGang() {
    return PEDTYPE_GANG1;
}

// ------------------------------------------------------------
// Runtime-only mutations (no territories.txt writes)
// ------------------------------------------------------------
void TerritorySystem::SetTerritoryOwner(const Territory* t, int newOwnerGang) {
    if (!t) return;

    for (auto& terr : s_territories) {
        if (terr.id == t->id) {
            terr.ownerGang = newOwnerGang;
            terr.underAttack = false;
            DebugLog::Write("TerritorySystem: %s owner=%d (runtime)", t->id.c_str(), newOwnerGang);
            break;
        }
    }
}

void TerritorySystem::SetUnderAttack(const Territory* t, bool underAttack) {
    if (!t) return;

    for (auto& terr : s_territories) {
        if (terr.id == t->id) {
            terr.underAttack = underAttack;
            DebugLog::Write("TerritorySystem: %s underAttack=%d (runtime)", t->id.c_str(), underAttack ? 1 : 0);
            break;
        }
    }
}

void TerritorySystem::ResetOwnershipToDefaults() {
    for (auto& t : s_territories) {
        t.ownerGang = t.defaultOwnerGang;
    }
}

void TerritorySystem::ApplyOwnershipState(const std::vector<OwnershipEntry>& entries) {
    for (const auto& e : entries) {
        for (auto& t : s_territories) {
            if (t.id == e.id) {
                t.ownerGang = e.ownerGang;
                break;
            }
        }
    }
}

void TerritorySystem::GetOwnershipState(std::vector<OwnershipEntry>& out) {
    out.clear();
    out.reserve(s_territories.size());
    for (const auto& t : s_territories) {
        OwnershipEntry e;
        e.id = t.id;
        e.ownerGang = t.ownerGang;
        out.push_back(e);
    }
}

void TerritorySystem::ClearAllWarsAndTransientState() {
    for (auto& t : s_territories) {
        t.underAttack = false;

        // If we ever add any other runtime-only fields, clear them here too.
        // t.attackStartMs = 0;
        // t.lastWarEndMs = 0;
        // t.pendingCapture = false;
        // etc...
    }
}

void TerritorySystem::ClearAllUnderAttackFlags() {
    for (Territory& t : s_territories) {
        if (t.underAttack) {
            t.underAttack = false;
            DebugLog::Write(
                "TerritorySystem: %s underAttack cleared due to load",
                t.id.c_str()
            );
        }
    }
}

const std::vector<Territory>& TerritorySystem::GetTerritories() {
    return s_territories;
}

void TerritorySystem::DrawRadarOverlay() {
    if (!s_overlayEnabled) return;
    TerritoryRadarRenderer::DrawRadarOverlay(s_territories);
}

// ------------------------------------------------------------
// Editor API (same keybinds as before)
// ------------------------------------------------------------
void TerritorySystem::EditorToggle() {
    s_editor.enabled = !s_editor.enabled;
    s_editor.hasA = false;
    s_editor.hasB = false;
    DebugLog::Write(s_editor.enabled ? "Territory editor: ON" : "Territory editor: OFF");
}

bool TerritorySystem::EditorEnabled() {
    return s_editor.enabled;
}

void TerritorySystem::EditorSetCornerAAtPlayer() {
    if (!s_editor.enabled) return;

    float x, y;
    if (!GetPlayerXY(x, y)) return;

    s_editor.ax = x;
    s_editor.ay = y;
    s_editor.hasA = true;
    s_editor.hasB = false;

    DebugLog::Write("Editor: Corner A set");
}

void TerritorySystem::EditorSetCornerBAtPlayer() {
    if (!s_editor.enabled) return;
    if (!s_editor.hasA) {
        DebugLog::Write("Editor: Set Corner A first");
        return;
    }

    float x, y;
    if (!GetPlayerXY(x, y)) return;

    s_editor.bx = x;
    s_editor.by = y;
    s_editor.hasB = true;

    DebugLog::Write("Editor: Corner B set");
}

void TerritorySystem::EditorCommitTerritory() {
    if (!s_editor.enabled) return;
    if (!s_editor.hasA || !s_editor.hasB) {
        DebugLog::Write("TerritoryEditor: Need A and B corners");
        return;
    }

    Territory t{};
    char idbuf[64];
    std::snprintf(idbuf, sizeof(idbuf), "%d", s_editor.nextId++);
    t.id = idbuf;

    t.minX = std::min(s_editor.ax, s_editor.bx);
    t.maxX = std::max(s_editor.ax, s_editor.bx);
    t.minY = std::min(s_editor.ay, s_editor.by);
    t.maxY = std::max(s_editor.ay, s_editor.by);

    t.defaultOwnerGang = s_editor.defaultOwnerGang;
    t.ownerGang = t.defaultOwnerGang;
    t.underAttack = false;
    t.defenseLevel = s_editor.defaultDefenseLevel;

    const float w = (t.maxX - t.minX);
    const float h = (t.maxY - t.minY);
    if (w < 2.0f || h < 2.0f) {
        DebugLog::Write("TerritoryEditor: Territory too small (%.1fx%.1f)", w, h);
        return;
    }

    s_territories.push_back(t);

    std::string err;
    if (!SaveToFile(s_territories, err)) {
        DebugLog::Write("TerritoryEditor: Save FAILED: %s", err.c_str());
        s_territories.pop_back();
        return;
    }

    s_lastConfigStamp = GetConfigStampOrNeg1();
    s_editor.hasA = false;
    s_editor.hasB = false;

    DebugLog::Write("TerritoryEditor: Territory %s saved successfully", t.id.c_str());
}

void TerritorySystem::EditorDeleteClosestToPlayer() {
    if (!s_editor.enabled) return;
    if (s_territories.empty()) {
        DebugLog::Write("Editor: No territories");
        return;
    }

    float px, py;
    if (!GetPlayerXY(px, py)) return;

    int bestIdx = -1;
    float bestD2 = 0.0f;

    for (int i = 0; i < (int)s_territories.size(); i++) {
        const auto& t = s_territories[i];
        const float cx = (t.minX + t.maxX) * 0.5f;
        const float cy = (t.minY + t.maxY) * 0.5f;
        const float d2 = Dist2(px, py, cx, cy);
        if (bestIdx < 0 || d2 < bestD2) { bestIdx = i; bestD2 = d2; }
    }

    if (bestIdx < 0) return;

    const std::string deletedId = s_territories[bestIdx].id;
    s_territories.erase(s_territories.begin() + bestIdx);

    std::string err;
    if (!SaveToFile(s_territories, err)) {
        DebugLog::Write("Editor: Delete save FAILED");
        return;
    }

    s_lastConfigStamp = GetConfigStampOrNeg1();
    DebugLog::Write("Editor: Deleted %s", deletedId.c_str());
}
