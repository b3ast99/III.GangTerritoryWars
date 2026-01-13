#include "PopulationAddPedHook.h"

#include "DebugLog.h"
#include "HookUtil.h"

#include "CTimer.h"
#include "TerritorySystem.h"
#include "GangInfo.h"

#include <Windows.h>
#include <cstdint>

// ------------------------------------------------------------
// Diagnostic globals
// ------------------------------------------------------------
extern "C" {
    volatile uint32_t g_PopAddPed_HitCount = 0;
    volatile uint32_t g_PopAddPed_GangHitCount = 0;
    volatile uint32_t g_PopAddPed_RewriteCount = 0;
    volatile uint32_t g_PopAddPed_LastPedType = 0;
    volatile uint32_t g_PopAddPed_LastModelArg = 0;
    volatile uint32_t g_PopAddPed_LastOwnerGang = 0;
}

// ------------------------------------------------------------
// Static state
// ------------------------------------------------------------
bool PopulationAddPedHook::s_installed = false;
bool PopulationAddPedHook::s_enabled = true;
uint32_t PopulationAddPedHook::s_hookedAddr = 0;
PopulationAddPedHook::AddPed_t PopulationAddPedHook::s_original = nullptr;

// IMPORTANT:
// In GTA III, AddPed second argument is "modelIndexOrCopType".
// It is NOT always safe to treat it as a model index.
// We only rewrite when the game is *already* trying to spawn a gang model.
static inline bool IsGangModelIndex(unsigned int modelIdx)
{
    return (modelIdx >= 10 && modelIdx <= 15);
}

bool PopulationAddPedHook::IsInstalled() { return s_installed; }
void PopulationAddPedHook::SetEnabled(bool enabled) { s_enabled = enabled; }
bool PopulationAddPedHook::IsEnabled() { return s_enabled; }

void PopulationAddPedHook::Install()
{
    DebugLog::Write("=== Installing PopulationAddPedHook (CPopulation::AddPed) ===");

    if (s_installed) {
        DebugLog::Write("PopulationAddPedHook already installed at 0x%08X", s_hookedAddr);
        return;
    }

    const uint32_t addr_10 = 0x004F5280; // GTA III 1.0 from your plugin-sdk snippet

    if (TryInstallAtAddress(addr_10)) {
        s_installed = true;
        s_hookedAddr = addr_10;
        DebugLog::Write("SUCCESS: PopulationAddPedHook installed at 0x%08X", addr_10);
        return;
    }

    DebugLog::Write("FAILED: PopulationAddPedHook install failed at 0x%08X", addr_10);
}

bool PopulationAddPedHook::TryInstallAtAddress(uint32_t addr)
{
    void* target = reinterpret_cast<void*>(addr);

    unsigned char* p = (unsigned char*)target;
    DebugLog::Write("PopulationAddPedHook target bytes %08X: %02X %02X %02X %02X %02X %02X %02X %02X",
        addr, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);

    // Prologue: 53 56 57 55 83 EC 58 ...
    // Must steal 7 bytes to include full "83 EC imm8".
    constexpr std::size_t kStolen = 7;

    void* tramp = HookUtil::MakeTrampoline(target, kStolen);
    if (!tramp) {
        DebugLog::Write("PopulationAddPedHook: trampoline alloc failed at 0x%08X", addr);
        return false;
    }

    if (!HookUtil::WriteRelJmp(target, (void*)&AddPedHook)) {
        DebugLog::Write("PopulationAddPedHook: WriteRelJmp failed at 0x%08X", addr);
        return false;
    }

    s_original = reinterpret_cast<AddPed_t>(tramp);
    DebugLog::Write("PopulationAddPedHook: trampoline at %p for 0x%08X (stolen=%u)", tramp, addr, (unsigned)kStolen);
    return true;
}

CPed* __cdecl PopulationAddPedHook::AddPedHook(ePedType pedType, unsigned int modelIndexOrCopType, const CVector& coors)
{
    g_PopAddPed_HitCount++;
    g_PopAddPed_LastPedType = (uint32_t)pedType;
    g_PopAddPed_LastModelArg = (uint32_t)modelIndexOrCopType;

    // We only rewrite "modelIndex" through paths that treat it as a model index (gang models).
    if (s_enabled && IsGangModelIndex(modelIndexOrCopType)) {

        const Territory* t = TerritorySystem::GetTerritoryAtPoint(coors);
        const int ownerGang = (t ? t->ownerGang : -1);
        g_PopAddPed_LastOwnerGang = (uint32_t)ownerGang;

        if (t && ownerGang >= (int)PEDTYPE_GANG1 && ownerGang <= (int)PEDTYPE_GANG9) {
            const ePedType owner = (ePedType)ownerGang;

            const int desiredModel = GangManager::GetRandomModelId(owner);
            if (desiredModel >= 0) {
                const bool modelDiff = ((unsigned)desiredModel != modelIndexOrCopType);
                const bool typeDiff = (pedType != owner);  // NEW: Check if type needs sync

                if (modelDiff || typeDiff) {  // NEW: Override if either differs
                    if (typeDiff) {
                        pedType = owner;  // Sync type to match new model/gang
                    }
                    modelIndexOrCopType = (unsigned)desiredModel;
                    g_PopAddPed_RewriteCount++;

                    static unsigned int s_nextLogMs = 0;
                    const unsigned int now = CTimer::m_snTimeInMilliseconds;
                    if (now >= s_nextLogMs) {
                        s_nextLogMs = now + 800;
                        DebugLog::Write(
                            "AddPed REWRITE(SAFE): terr=%s owner=%d pos(%.1f,%.1f,%.1f) -> type=%d model=%u (typeDiff=%d modelDiff=%d)",
                            t->id.c_str(),
                            ownerGang,
                            coors.x, coors.y, coors.z,
                            (int)pedType,
                            (unsigned)modelIndexOrCopType,
                            (int)typeDiff,
                            (int)modelDiff
                        );
                    }
                }
            }
        }
    }
    else {
        g_PopAddPed_LastOwnerGang = 0xFFFFFFFFu;
    }

    return s_original ? s_original(pedType, modelIndexOrCopType, coors) : nullptr;
}

void PopulationAddPedHook::DebugTick()
{
    if (!s_installed) return;

    static unsigned int nextMs = 0;
    const unsigned int now = CTimer::m_snTimeInMilliseconds;
    if (now < nextMs) return;
    nextMs = now + 2000;

    DebugLog::Write("AddPed stats: hit=%u rewrite=%u lastType=%u lastArg=%u lastOwner=%d",
        (unsigned)g_PopAddPed_HitCount,
        (unsigned)g_PopAddPed_RewriteCount,
        (unsigned)g_PopAddPed_LastPedType,
        (unsigned)g_PopAddPed_LastModelArg,
        (int)g_PopAddPed_LastOwnerGang
    );
}
