#include "TerritorySystem.h"
#include "DebugLog.h"

#include "CRadar.h"
#include "CWorld.h"
#include "CPlayerPed.h"
#include "CTheScripts.h"
#include "CMessages.h"
#include "CTimer.h"

#include "TerritoryRadarRenderer.h"

#include <cstdio>
#include <algorithm>
#include <sys/stat.h>
#include <cctype>
#include <windows.h>

// -----------------------------------------------------------------------------
// territories.txt (single source of truth)
//
// CSV-ish format:
//
//   id,minX,minY,maxX,maxY[,ownerGangCode[,underAttack[,defenseLevel]]]
//
// where ownerGangCode:
//   -1 = neutral
//    7 = PEDTYPE_GANG1
//    8 = PEDTYPE_GANG2
//    9 = PEDTYPE_GANG3
// 
// where defenseLevel:
//    0 = light
//    1 = moderate
//    2 = heavy
//
// Examples:
//   1001,902,-327,961,-160.5,9,0,0
//   1002,-1000,700,-800,1000,8,0,1
//   1003,-600,550,-800,750,7,0,2

//
// Rules:
// - id cannot contain commas
// - ownerGang defaults to -1 if omitted
// - underAttack defaults to 0 if omitted
// - defenseLevel defaults to 1 if omitted
// - Lines starting with # are comments
// - Empty lines ignored
// -----------------------------------------------------------------------------

static CVector TerritoryCenter3D(const Territory& t) {
    return CVector((t.minX + t.maxX) * 0.5f, (t.minY + t.maxY) * 0.5f, 0.0f);
}

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

static int ParseIntStrict(const std::string& s, bool& ok)
{
    ok = false;
    if (!IsAllDigits(s)) return 0;
    long long v = 0;
    for (char c : s) {
        v = v * 10 + (c - '0');
        if (v > 2000000000LL) return 0;
    }
    ok = true;
    return (int)v;
}


static void TrimInPlace(char* s) {
    if (!s) return;

    // leading
    while (*s && std::isspace((unsigned char)*s)) ++s;

    // We can't shift pointer for caller, so do manual left trim into buffer:
    // (This is only used for line buffers we own.)
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

        // Strip filename
        char* lastSlash = strrchr(modulePath, '\\');
        if (lastSlash) {
            *(lastSlash + 1) = '\0';
        }

        // Append territories.txt to the same directory
        size_t moduleLen = strlen(modulePath);
        size_t needed = moduleLen + strlen("territories.txt") + 1;
        if (needed <= sizeof(path)) {
            snprintf(path, sizeof(path), "%sterritories.txt", modulePath);
        }
        else {
            // Fallback
            DebugLog::Write("Path too long, using current directory");
            strcpy_s(path, sizeof(path), "territories.txt");
        }

        DebugLog::Write("Territory config path: %s", path);
        initialized = true;
    }

    return path;
}

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
    // st_mtime is seconds; plenty for our hot reload needs.
    return (long long)s.st_mtime;
}

// TerritorySystem.cpp - Update ParseLineTerritory function
static bool ParseLineTerritory(const char* line, Territory& outT, std::string& outErr) {
    // Expect: id,minX,minY,maxX,maxY[,ownerGang[,underAttack[,defenseLevel]]]
    // id cannot contain commas, so scan id up to first comma.
    if (!line || !line[0]) { outErr = "Empty line"; return false; }

    // Make a local mutable copy (safe tokenization)
    char buf[512];
    std::snprintf(buf, sizeof(buf), "%s", line);

    // Tokenize by commas - increase from 7 to 8 tokens
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

    // Trim whitespace on each token
    for (int i = 0; i < count; i++) {
        char* t = LTrim(tokens[i]);
        // shift left by copying into same spot
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

    // Require numeric id in file
    if (!IsAllDigits(t.id)) {
        outErr = "Id must be numeric (e.g. 1001)";
        return false;
    }

    // Parse floats
    if (std::sscanf(tokens[1], "%f", &t.minX) != 1) { outErr = "Bad minX"; return false; }
    if (std::sscanf(tokens[2], "%f", &t.minY) != 1) { outErr = "Bad minY"; return false; }
    if (std::sscanf(tokens[3], "%f", &t.maxX) != 1) { outErr = "Bad maxX"; return false; }
    if (std::sscanf(tokens[4], "%f", &t.maxY) != 1) { outErr = "Bad maxY"; return false; }

    t.ownerGang = -1;
    t.underAttack = false;
    t.defenseLevel = 1; // Default to moderate

    if (count >= 6 && tokens[5] && tokens[5][0]) {
        int code = -1;
        if (std::sscanf(tokens[5], "%d", &code) != 1) { outErr = "Bad ownerGangCode"; return false; }
        t.ownerGang = code;
    }

    if (count >= 7 && tokens[6] && tokens[6][0]) {
        int ua = 0;
        if (std::sscanf(tokens[6], "%d", &ua) != 1) { outErr = "Bad underAttack"; return false; }
        t.underAttack = (ua != 0);
    }

    // NEW: Parse defenseLevel (0=Light, 1=Moderate, 2=Heavy)
    if (count >= 8 && tokens[7] && tokens[7][0]) {
        int dl = 1; // Default to moderate
        if (std::sscanf(tokens[7], "%d", &dl) != 1) { outErr = "Bad defenseLevel"; return false; }
        // Clamp to valid range
        if (dl < 0) dl = 0;
        if (dl > 2) dl = 2;
        t.defenseLevel = dl;
    }

    // Normalize
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
            char buf[256];
            std::snprintf(buf, sizeof(buf), "Parse error line %d: %s", lineNo, perr.c_str());
            outErr = buf;
            std::fclose(f);
            return false;
        }

        // Enforce unique ids
        auto dup = std::find_if(out.begin(), out.end(), [&](const Territory& x) { return x.id == t.id; });
        if (dup != out.end()) {
            char buf[256];
            std::snprintf(buf, sizeof(buf), "Duplicate id '%s' at line %d", t.id.c_str(), lineNo);
            outErr = buf;
            std::fclose(f);
            return false;
        }

        out.push_back(std::move(t));
    }

    std::fclose(f);

    if (out.empty()) {
        outErr = "No territories loaded";
        return false;
    }

    return true;
}

static bool AtomicWriteTerritories(const char* finalPath, const char* tmpPath, const char* bakPath,
    const std::vector<Territory>& terrs, std::string& outErr) {

    DebugLog::Write("TerritorySystem: Opening temp file %s", tmpPath);

    FILE* f = std::fopen(tmpPath, "wb");
    if (!f) {
        outErr = "Failed to open temp file for write";
        DebugLog::Write("TerritorySystem: Failed to open temp file %s, error: %d", tmpPath, errno);
        return false;
    }

    DebugLog::Write("TerritorySystem: Writing %d territories", (int)terrs.size());

    std::fprintf(f, "# id,minX,minY,maxX,maxY,ownerGangCode,underAttack\n");
    for (const auto& t : terrs) {
        std::fprintf(
            f,
            "%s,%.3f,%.3f,%.3f,%.3f,%d,%d,%d\n",
            t.id.c_str(),
            t.minX, t.minY, t.maxX, t.maxY,
            t.ownerGang,
            t.underAttack ? 1 : 0,
            t.defenseLevel
        );
    }

    std::fclose(f);
    DebugLog::Write("TerritorySystem: Temp file written successfully");

    // Simple atomic-ish replace:
    // 1. Remove old backup if exists
    std::remove(bakPath);

    // 2. Backup current file if it exists
    if (std::rename(finalPath, bakPath) != 0) {
        // It's OK if this fails - file might not exist yet
        DebugLog::Write("TerritorySystem: Note: Could not create backup (file may not exist)");
    }

    // 3. Move temp to final
    if (std::rename(tmpPath, finalPath) != 0) {
        outErr = "Failed to move temp file into place";
        DebugLog::Write("TerritorySystem: Failed to rename temp->final: error=%d", errno);

        // Try copy as fallback
        std::ifstream src(tmpPath, std::ios::binary);
        std::ofstream dst(finalPath, std::ios::binary);
        if (src && dst) {
            dst << src.rdbuf();
            DebugLog::Write("TerritorySystem: Used copy fallback");
            std::remove(tmpPath); // Clean up temp file
            return true;
        }
        else {
            DebugLog::Write("TerritorySystem: Copy fallback also failed");
            return false;
        }
    }

    DebugLog::Write("TerritorySystem: Save completed successfully");
    return true;
}

bool TerritorySystem::SaveToFile(const std::vector<Territory>& terrs, std::string& outErr) {
    // sort by id for deterministic output
    std::vector<Territory> sorted = terrs;
    std::sort(sorted.begin(), sorted.end(), [](const Territory& a, const Territory& b) {
        bool oka = false, okb = false;
        int ia = ParseIntStrict(a.id, oka);
        int ib = ParseIntStrict(b.id, okb);

        // numeric-first; fall back to string if something slipped through
        if (oka && okb) return ia < ib;
        if (oka != okb) return oka; // numeric ids before non-numeric
        return a.id < b.id;
        });

    // NO TEMP DIRECTORY NEEDED - everything goes in same directory
    const char* finalPath = ConfigPath();

    // Create temp and backup files in the SAME directory
    // Use the final path to create temp/bak paths
    char tmpPath[MAX_PATH];
    char bakPath[MAX_PATH];

    // Create temp file name by appending .tmp to the final path
    std::snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", finalPath);

    // Create backup file name by appending .bak to the final path
    std::snprintf(bakPath, sizeof(bakPath), "%s.bak", finalPath);

    DebugLog::Write("TerritorySystem: Saving to %s (tmp=%s, bak=%s)",
        finalPath, tmpPath, bakPath);

    return AtomicWriteTerritories(finalPath, tmpPath, bakPath, sorted, outErr);
}

int TerritorySystem::ComputeNextId(const std::vector<Territory>& terrs) {
    int best = 0;

    for (const auto& t : terrs) {
        bool ok = false;
        int id = ParseIntStrict(t.id, ok);
        if (ok && id > best) best = id;
    }

    if (best < 1000) best = 1000;
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
    std::vector<Territory> next;
    std::string err;

    if (!LoadFromFile(next, err)) {
        const unsigned int now = CTimer::m_snTimeInMilliseconds;
        // rate limit fail toasts
        if (showToastOnFail && (now - s_lastReloadFailToastMs) > 2000) {
            s_lastReloadFailToastMs = now;
            DebugLog::Write("TerritorySystem: Reload failed: %s", err.c_str());
        }
        return;
    }

    s_territories.swap(next);
    s_editor.nextId = ComputeNextId(s_territories);

    DebugLog::Write("TerritorySystem: Reloaded %d territories", (int)s_territories.size());

    // stamp update
    s_lastConfigStamp = GetConfigStampOrNeg1();
}

void TerritorySystem::ForceReloadNow() {
    TryReloadNow(true);
}

void TerritorySystem::HotReloadTick(unsigned int nowMs) {
    if (nowMs < s_nextReloadPollMs) return;
    s_nextReloadPollMs = nowMs + 1000;

    const long long stamp = GetConfigStampOrNeg1();
    if (stamp < 0) return; // file missing -> don't spam

    if (s_lastConfigStamp < 0) {
        s_lastConfigStamp = stamp;
        return;
    }

    if (stamp != s_lastConfigStamp) {
        // file changed - attempt reload (toast on fail)
        TryReloadNow(true);
    }
}

void TerritorySystem::Init() {
    s_territories.clear();
    s_overlayEnabled = true;

    s_nextReloadPollMs = 0;
    s_lastReloadFailToastMs = 0;

    // initial load
    TryReloadNow(true);

    // editor defaults
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

    // No key handling here. Main calls editor functions.
    // Nothing else needed in Update right now.
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
    // Placeholder: keep your scripts-based check behavior stable.
    //if (CTheScripts::ScriptSpace[0x1A4] > 10) {
    //    return PEDTYPE_GANG1; // Leone Mafia
    //}
    //return -1;

    return PEDTYPE_GANG1;
}

void TerritorySystem::SetTerritoryOwner(const Territory* t, int newOwnerGang) {
    if (!t) return;

    for (auto& terr : s_territories) {
        if (terr.id == t->id) {
            terr.ownerGang = newOwnerGang;
            terr.underAttack = false;

            std::string err;
            if (!SaveToFile(s_territories, err)) {
                // Log to debug instead of showing UI popup
                DebugLog::Write("TerritorySystem: FAILED to save territory %s: %s",
                    t->id.c_str(), err.c_str());
            }
            else {
                // reload stamp so hot reload doesn't fight us
                s_lastConfigStamp = GetConfigStampOrNeg1();

                // Debug log the capture instead of UI popup
                DebugLog::Write("TerritorySystem: %s captured (owner=%d)",
                    t->id.c_str(), newOwnerGang);
            }
            break;
        }
    }
}

void TerritorySystem::SetUnderAttack(const Territory* t, bool underAttack) {
    if (!t) return;

    for (auto& terr : s_territories) {
        if (terr.id == t->id) {
            terr.underAttack = underAttack;

            std::string err;
            if (!SaveToFile(s_territories, err)) {
                DebugLog::Write("TerritorySystem: FAILED to save underAttack for %s: %s",
                    t->id.c_str(), err.c_str());
            }
            else {
                s_lastConfigStamp = GetConfigStampOrNeg1();
                DebugLog::Write("TerritorySystem: %s underAttack=%d",
                    t->id.c_str(), underAttack);
            }
            break;
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

// -----------------------------------------------------------------------------
// Editor API (called from Main.cpp via Ctrl+keybinds)
// -----------------------------------------------------------------------------

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
    // Deterministic new id
    char idbuf[64];
    std::snprintf(idbuf, sizeof(idbuf), "%d", s_editor.nextId++);
    t.id = idbuf;

    t.minX = std::min(s_editor.ax, s_editor.bx);
    t.maxX = std::max(s_editor.ax, s_editor.bx);
    t.minY = std::min(s_editor.ay, s_editor.by);
    t.maxY = std::max(s_editor.ay, s_editor.by);

    t.ownerGang = s_editor.defaultOwnerGang;
    t.underAttack = false;
    t.defenseLevel = s_editor.defaultDefenseLevel;

    // Basic sanity: prevent zero-area boxes
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
        // rollback in-memory add if save fails
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

    // delete
    const std::string deletedId = s_territories[bestIdx].id;
    s_territories.erase(s_territories.begin() + bestIdx);

    std::string err;
    if (!SaveToFile(s_territories, err)) {
        DebugLog::Write("Editor: Delete save FAILED");
        return;
    }

    s_lastConfigStamp = GetConfigStampOrNeg1();

    char msg[128];
    std::snprintf(msg, sizeof(msg), "Editor: Deleted %s", deletedId.c_str());
    DebugLog::Write(msg);
}
