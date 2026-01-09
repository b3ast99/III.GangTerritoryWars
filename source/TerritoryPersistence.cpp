#include "TerritoryPersistence.h"
#include "HookUtil.h"

#include "TerritorySystem.h"
#include "WaveManager.h"
#include "DebugLog.h"
#include "CTimer.h"
#include "CMenuManager.h"

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <vector>
#include <direct.h>

// ------------------------------------------------------------
// Static state
// ------------------------------------------------------------
bool TerritoryPersistence::s_inited = false;
bool TerritoryPersistence::s_installed = false;

TerritoryPersistence::OpenFile_t  TerritoryPersistence::s_originalOpen = nullptr;
TerritoryPersistence::CloseFile_t TerritoryPersistence::s_originalClose = nullptr;

TerritoryPersistence::HandleOp TerritoryPersistence::s_ops[64]{};
bool TerritoryPersistence::s_used[64]{};

int TerritoryPersistence::s_pendingApplySlot = -1;
int TerritoryPersistence::s_pendingWriteSlot = -1;

static int s_lastAppliedSlot = -1;
static unsigned int s_lastAppliedMs = 0;

static int s_lastSavedSlot = -1;
static unsigned int s_lastSavedMs = 0;


// Sidecar format
static const unsigned int kMagic = 0x31575447; // 'GTW1'
static const unsigned int kVersion = 1;

static void PushU32(std::vector<unsigned char>& out, unsigned int v) {
    out.push_back((unsigned char)(v & 0xFF));
    out.push_back((unsigned char)((v >> 8) & 0xFF));
    out.push_back((unsigned char)((v >> 16) & 0xFF));
    out.push_back((unsigned char)((v >> 24) & 0xFF));
}

static bool ReadU32(const std::vector<unsigned char>& b, size_t& i, unsigned int& v) {
    if (i + 4 > b.size()) return false;
    v = (unsigned int)b[i]
        | ((unsigned int)b[i + 1] << 8)
        | ((unsigned int)b[i + 2] << 16)
        | ((unsigned int)b[i + 3] << 24);
    i += 4;
    return true;
}

static void LogOwnershipEntries(const char* tag, const std::vector<TerritorySystem::OwnershipEntry>& entries)
{
    int found1001 = 0;
    int owner1001 = -1;

    for (const auto& e : entries) {
        if (e.id == "1001") {
            found1001 = 1;
            owner1001 = e.ownerGang;
            break;
        }
    }

    DebugLog::Write("%s: entries=%d, 1001_found=%d, 1001_owner=%d",
        tag, (int)entries.size(), found1001, owner1001);

    // Optional: dump all (only 8 territories so its fine)
    for (const auto& e : entries) {
        DebugLog::Write("%s: id=%s owner=%d", tag, e.id.c_str(), e.ownerGang);
    }
}


// ------------------------------------------------------------
// Front-end flow gating (prevents menu preview reads from applying)
// ------------------------------------------------------------
static bool IsLoadFlow()
{
    if (!FrontEndMenuManager.m_bMenuActive) return false;

    if (FrontEndMenuManager.m_bWantToLoad) return true;

    const int p = FrontEndMenuManager.m_nCurrentMenuPage;
    return p == MENUPAGE_CHOOSE_LOAD_SLOT
        || p == MENUPAGE_LOAD_SLOT_CONFIRM
        || p == MENUPAGE_LOADING_IN_PROGRESS;
}

static bool IsSaveFlow()
{
    if (!FrontEndMenuManager.m_bMenuActive) return false;

    if (FrontEndMenuManager.m_bSaveMenuActive) return true;

    const int p = FrontEndMenuManager.m_nCurrentMenuPage;
    return p == MENUPAGE_CHOOSE_SAVE_SLOT
        || p == MENUPAGE_SAVE_OVERWRITE_CONFIRM
        || p == MENUPAGE_SAVING_IN_PROGRESS
        || p == MENUPAGE_SAVE;
}

static void DumpBytes(const char* name, void* addr, int count)
{
    unsigned char* p = (unsigned char*)addr;
    char line[256];
    char* w = line;
    w += sprintf(w, "%s %p :", name, addr);
    for (int i = 0; i < count; ++i) w += sprintf(w, " %02X", p[i]);
    DebugLog::Write("%s", line);
}

// ------------------------------------------------------------
// Path helpers
// ------------------------------------------------------------
const char* TerritoryPersistence::GetAsiDir() {
    static char dir[MAX_PATH];
    static bool inited = false;
    if (inited) return dir;

    HMODULE hMod = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)&TerritoryPersistence::GetAsiDir,
        &hMod
    );

    char modulePath[MAX_PATH];
    GetModuleFileNameA(hMod, modulePath, MAX_PATH);

    char* lastSlash = strrchr(modulePath, '\\');
    if (lastSlash) *(lastSlash + 1) = '\0';

    strcpy_s(dir, sizeof(dir), modulePath);
    inited = true;
    return dir;
}

void TerritoryPersistence::EnsureDirExists(const char* path) {
    if (!path || !path[0]) return;
    _mkdir(path); // best effort
}

bool TerritoryPersistence::IsReadMode(const char* mode) {
    if (!mode) return false;
    const bool hasR = (std::strchr(mode, 'r') != nullptr);
    const bool hasW = (std::strchr(mode, 'w') != nullptr);
    const bool hasA = (std::strchr(mode, 'a') != nullptr);
    return hasR && !hasW && !hasA;
}

bool TerritoryPersistence::IsWriteMode(const char* mode) {
    if (!mode) return false;
    return (std::strchr(mode, 'w') != nullptr) || (std::strchr(mode, 'a') != nullptr);
}

bool TerritoryPersistence::TryParseSaveSlotFromPath(const char* filePath, int& outSlot) {
    if (!filePath) return false;

    // Look for GTA3sf{N}.b
    const char* key = "GTA3sf";
    const char* p = std::strstr(filePath, key);
    if (!p) return false;
    p += (int)std::strlen(key);

    if (!std::isdigit((unsigned char)*p)) return false;

    int slot = 0;
    while (std::isdigit((unsigned char)*p)) {
        slot = slot * 10 + (*p - '0');
        ++p;
    }

    if (slot < 1 || slot > 8) return false;

    // Require exact ".b" extension (avoid matching ".bak", etc.)
    const char* ext = std::strrchr(filePath, '.');
    if (!ext || _stricmp(ext, ".b") != 0) return false;

    outSlot = slot;
    return true;

}

// ------------------------------------------------------------
// Tracking
// ------------------------------------------------------------
void TerritoryPersistence::Track(FILESTREAM h, int slot, bool isSave, bool isLoad) {
    for (int i = 0; i < 64; ++i) {
        if (!s_used[i]) {
            s_used[i] = true;
            s_ops[i] = HandleOp{ h, slot, isSave, isLoad };
            return;
        }
    }
}

bool TerritoryPersistence::Untrack(FILESTREAM h, HandleOp& out) {
    for (int i = 0; i < 64; ++i) {
        if (s_used[i] && s_ops[i].h == h) {
            out = s_ops[i];
            s_used[i] = false;
            s_ops[i].h = 0;
            return true;
        }
    }
    return false;
}

// ------------------------------------------------------------
// Hook install (DamageHook style)
// ------------------------------------------------------------
bool TerritoryPersistence::TryInstall() {
    // NOTE:
    // CFileMgr::OpenFile prologue here uses pushes + call. Steal enough bytes to not cut instructions.
    // You already tested 10/5 works on your build.
    constexpr std::size_t kStolenOpen = 10;
    constexpr std::size_t kStolenClose = 5;

    void* openAddr = (void*)gaddrof(CFileMgr::OpenFile);
    void* closeAddr = (void*)gaddrof(CFileMgr::CloseFile);

    DumpBytes("OpenFile bytes", openAddr, 16);
    DumpBytes("CloseFile bytes", closeAddr, 16);

    void* openTramp = HookUtil::MakeTrampoline(openAddr, kStolenOpen);
    if (!openTramp) {
        DebugLog::Write("TerritoryPersistence: OpenFile trampoline alloc failed");
        return false;
    }
    if (!HookUtil::WriteRelJmp(openAddr, (void*)&OpenFileHook)) {
        DebugLog::Write("TerritoryPersistence: OpenFile WriteRelJmp failed");
        return false;
    }
    s_originalOpen = (OpenFile_t)openTramp;

    void* closeTramp = HookUtil::MakeTrampoline(closeAddr, kStolenClose);
    if (!closeTramp) {
        DebugLog::Write("TerritoryPersistence: CloseFile trampoline alloc failed");
        return false;
    }
    if (!HookUtil::WriteRelJmp(closeAddr, (void*)&CloseFileHook)) {
        DebugLog::Write("TerritoryPersistence: CloseFile WriteRelJmp failed");
        return false;
    }
    s_originalClose = (CloseFile_t)closeTramp;

    DebugLog::Write("TerritoryPersistence: Hooks installed (OpenFile=%p CloseFile=%p)", openAddr, closeAddr);
    return true;
}

// ------------------------------------------------------------
// Public API
// ------------------------------------------------------------
void TerritoryPersistence::Init() {
    if (s_inited) return;
    s_inited = true;

    char persistDir[MAX_PATH];
    std::snprintf(persistDir, sizeof(persistDir), "%spersistence", GetAsiDir());
    EnsureDirExists(persistDir);

    std::memset(s_used, 0, sizeof(s_used));
    s_pendingApplySlot = -1;
    s_pendingWriteSlot = -1;

    s_installed = TryInstall();
    DebugLog::Write("TerritoryPersistence: Init done installed=%d", s_installed ? 1 : 0);
}

void TerritoryPersistence::Shutdown() {
    s_inited = false;
}

void TerritoryPersistence::Process() {
    if (!s_inited) return;

    const unsigned int now = CTimer::m_snTimeInMilliseconds;

    if (s_pendingApplySlot != -1) {
        if (!FrontEndMenuManager.m_bMenuActive) { // wait until back in-game
            int slot = s_pendingApplySlot;
            s_pendingApplySlot = -1;

            if (slot == s_lastAppliedSlot && (now - s_lastAppliedMs) < 1500) {
                DebugLog::Write("TerritoryPersistence: skip duplicate apply slot %d", slot);
            } else {
                s_lastAppliedSlot = slot;
                s_lastAppliedMs = now;
                LoadSidecarAndApply(slot);
            }
        }
    }

    if (s_pendingWriteSlot != -1) {
        int slot = s_pendingWriteSlot;
        s_pendingWriteSlot = -1;

        if (slot == s_lastSavedSlot && (now - s_lastSavedMs) < 1500) {
            DebugLog::Write("TerritoryPersistence: skip duplicate save slot %d", slot);
        } else {
            s_lastSavedSlot = slot;
            s_lastSavedMs = now;
            SaveSidecar(slot);
        }
    }
}

// ------------------------------------------------------------
// Hook bodies
// ------------------------------------------------------------
FILESTREAM __cdecl TerritoryPersistence::OpenFileHook(const char* filePath, const char* mode)
{
    FILESTREAM h = s_originalOpen(filePath, mode);
    if (!h || !filePath || !mode) return h;

    int slot = 0;
    if (!TryParseSaveSlotFromPath(filePath, slot)) return h;

    const bool isRead = IsReadMode(mode);
    const bool isWrite = IsWriteMode(mode);

    bool willLoad = false;
    bool willSave = false;

    if (isRead) {
        // Only arm load if the game is actually intending to load (strongest signal).
        if (FrontEndMenuManager.m_bMenuActive && FrontEndMenuManager.m_bWantToLoad) {
            willLoad = true;
            DebugLog::Write("TerritoryPersistence: arm LOAD slot %d (page=%d wantLoad=%d)",
                slot, FrontEndMenuManager.m_nCurrentMenuPage, (int)FrontEndMenuManager.m_bWantToLoad);
        }
        else {
            DebugLog::Write("TerritoryPersistence: ignore read slot %d (not load intent, page=%d)",
                slot, FrontEndMenuManager.m_nCurrentMenuPage);
        }
    }


    if (isWrite) {
        // This is the safest signal for actual saving, but we'll also log if it doesn't look like save flow.
        if (IsSaveFlow()) {
            willSave = true;
            DebugLog::Write("TerritoryPersistence: arm SAVE slot %d (page=%d saveMenu=%d)",
                slot, FrontEndMenuManager.m_nCurrentMenuPage, (int)FrontEndMenuManager.m_bSaveMenuActive);
        }
        else {
            // Still track it; some builds can write outside obvious menu pages.
            willSave = true;
            DebugLog::Write("TerritoryPersistence: arm SAVE slot %d (write mode but not save flow, page=%d)",
                slot, FrontEndMenuManager.m_nCurrentMenuPage);
        }
    }

    if (willLoad || willSave) {
        Track(h, slot, willSave, willLoad);
    }

    return h;
}

int __cdecl TerritoryPersistence::CloseFileHook(FILESTREAM fileHandle)
{
    HandleOp op{};
    if (fileHandle && Untrack(fileHandle, op)) {
        if (op.isLoad) OnLoadCompleted(op.slot);
        if (op.isSave) OnSaveCompleted(op.slot);
    }

    return s_originalClose(fileHandle);
}

// ------------------------------------------------------------
// Completion handlers
// ------------------------------------------------------------
void TerritoryPersistence::OnSaveCompleted(int slot) {
    s_pendingWriteSlot = slot;
}

void TerritoryPersistence::OnLoadCompleted(int slot) {
    // Extra guard in case something weird calls into us:
    if (!IsLoadFlow()) {
        DebugLog::Write("TerritoryPersistence: OnLoadCompleted ignored (not load flow)");
        return;
    }

    if (WaveManager::IsWarActive()) {
        WaveManager::CancelWar();
    }

    s_pendingApplySlot = slot;
}

// ------------------------------------------------------------
// Sidecar IO (binary, stored in ASI folder)
// ------------------------------------------------------------
void TerritoryPersistence::LoadSidecarAndApply(int slot) {
    char path[MAX_PATH];
    std::snprintf(path, sizeof(path), "%spersistence\\slot_%d.dat", GetAsiDir(), slot);

    FILE* f = std::fopen(path, "rb");
    if (!f) {
        DebugLog::Write("TerritoryPersistence: no sidecar for slot %d -> defaults", slot);
        TerritorySystem::ResetOwnershipToDefaults();
        TerritorySystem::ClearAllWarsAndTransientState();
        return;
    }

    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);

    if (sz <= 0 || sz > 1024 * 1024) {
        std::fclose(f);
        DebugLog::Write("TerritoryPersistence: invalid sidecar size slot %d", slot);
        TerritorySystem::ResetOwnershipToDefaults();
        TerritorySystem::ClearAllWarsAndTransientState();
        return;
    }

    std::vector<unsigned char> bytes((size_t)sz);
    size_t rd = std::fread(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);

    if (rd != bytes.size()) {
        DebugLog::Write("TerritoryPersistence: sidecar read failed slot %d", slot);
        TerritorySystem::ResetOwnershipToDefaults();
        TerritorySystem::ClearAllWarsAndTransientState();
        return;
    }

    size_t i = 0;
    unsigned int magic = 0, ver = 0, count = 0;
    if (!ReadU32(bytes, i, magic) || !ReadU32(bytes, i, ver) || !ReadU32(bytes, i, count)) {
        DebugLog::Write("TerritoryPersistence: corrupt header slot %d", slot);
        TerritorySystem::ResetOwnershipToDefaults();
        TerritorySystem::ClearAllWarsAndTransientState();
        return;
    }

    if (magic != kMagic || ver != kVersion || count > 4096) {
        DebugLog::Write("TerritoryPersistence: bad magic/ver slot %d", slot);
        TerritorySystem::ResetOwnershipToDefaults();
        TerritorySystem::ClearAllWarsAndTransientState();
        return;
    }

    std::vector<TerritorySystem::OwnershipEntry> entries;
    entries.reserve(count);

    bool corrupt = false;

    for (unsigned int n = 0; n < count; ++n) {
        if (i + 2 > bytes.size()) { corrupt = true; break; }

        unsigned short len = (unsigned short)bytes[i] | ((unsigned short)bytes[i + 1] << 8);
        i += 2;
        if (i + len > bytes.size()) { corrupt = true; break; }

        TerritorySystem::OwnershipEntry e;
        e.id.assign((const char*)&bytes[i], (size_t)len);
        i += len;

        unsigned int ownerU = 0;
        if (!ReadU32(bytes, i, ownerU)) { corrupt = true; break; }

        e.ownerGang = (int)ownerU;
        entries.push_back(e);
    }

    if (corrupt || entries.size() != count) {
        DebugLog::Write("TerritoryPersistence: corrupt payload slot %d (parsed=%d expected=%u)",
            slot, (int)entries.size(), count);
        TerritorySystem::ResetOwnershipToDefaults();
        TerritorySystem::ClearAllWarsAndTransientState();
        return;
    }

    LogOwnershipEntries("TerritoryPersistence: LOAD sidecar", entries);

    TerritorySystem::ResetOwnershipToDefaults();
    TerritorySystem::ApplyOwnershipState(entries);
    TerritorySystem::ClearAllWarsAndTransientState();

    DebugLog::Write("TerritoryPersistence: applied slot %d entries=%d", slot, (int)entries.size());

}

void TerritoryPersistence::SaveSidecar(int slot) {
    char persistDir[MAX_PATH];
    std::snprintf(persistDir, sizeof(persistDir), "%spersistence", GetAsiDir());
    EnsureDirExists(persistDir);

    char finalPath[MAX_PATH];
    char tmpPath[MAX_PATH];
    std::snprintf(finalPath, sizeof(finalPath), "%s\\slot_%d.dat", persistDir, slot);
    std::snprintf(tmpPath, sizeof(tmpPath), "%s\\slot_%d.dat.tmp", persistDir, slot);

    std::vector<TerritorySystem::OwnershipEntry> entries;
    TerritorySystem::GetOwnershipState(entries);

    LogOwnershipEntries("TerritoryPersistence: SAVE snapshot", entries);

    std::vector<unsigned char> out;
    out.reserve(16 + entries.size() * 16);

    PushU32(out, kMagic);
    PushU32(out, kVersion);
    PushU32(out, (unsigned int)entries.size());

    for (auto& e : entries) {
        unsigned short len = (unsigned short)e.id.size();
        out.push_back((unsigned char)(len & 0xFF));
        out.push_back((unsigned char)((len >> 8) & 0xFF));
        out.insert(out.end(), e.id.begin(), e.id.end());
        PushU32(out, (unsigned int)e.ownerGang);
    }

    FILE* f = std::fopen(tmpPath, "wb");
    if (!f) {
        DebugLog::Write("TerritoryPersistence: failed open tmp slot %d", slot);
        return;
    }

    size_t wr = std::fwrite(out.data(), 1, out.size(), f);
    std::fclose(f);

    if (wr != out.size()) {
        DebugLog::Write("TerritoryPersistence: failed write tmp slot %d", slot);
        std::remove(tmpPath);
        return;
    }

    std::remove(finalPath);
    if (std::rename(tmpPath, finalPath) != 0) {
        DebugLog::Write("TerritoryPersistence: rename tmp->final failed slot %d", slot);
        std::remove(tmpPath);
        return;
    }

    DebugLog::Write("TerritoryPersistence: saved slot %d entries=%d", slot, (int)entries.size());
}
