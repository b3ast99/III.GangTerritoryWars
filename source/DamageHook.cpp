#include "DamageHook.h"
#include "DirectDamageTracker.h"
#include "DebugLog.h"

#include "CPed.h"
#include "CPlayerPed.h"
#include "CWorld.h"
#include "CTimer.h"
#include "CEntity.h"

#include <Windows.h>
#include <cstdint>
#include <vector>
#include <cstring>

// ------------------------------------------------------------
// Static state
// ------------------------------------------------------------
bool DamageHook::s_installed = false;
unsigned int DamageHook::s_hookedAddr = 0;
DamageHook::InflictDamage_t DamageHook::s_original = nullptr;

// ------------------------------------------------------------
// Small helpers
// ------------------------------------------------------------
static bool WriteRelJmp(void* src, void* dst)
{
    DWORD oldProtect{};
    if (!VirtualProtect(src, 5, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    auto* p = reinterpret_cast<std::uint8_t*>(src);
    p[0] = 0xE9; // JMP rel32
    std::int32_t rel = (std::int32_t)((std::uint8_t*)dst - ((std::uint8_t*)src + 5));
    std::memcpy(p + 1, &rel, sizeof(rel));

    VirtualProtect(src, 5, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), src, 5);
    return true;
}

static void* MakeTrampoline(void* target, std::size_t stolenBytes)
{
    // Allocate RWX memory for trampoline
    void* tramp = VirtualAlloc(nullptr, stolenBytes + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) return nullptr;

    // Copy stolen bytes from original function entry
    std::memcpy(tramp, target, stolenBytes);

    // Add a jump back to original+stolenBytes
    std::uint8_t* jmpFrom = (std::uint8_t*)tramp + stolenBytes;
    jmpFrom[0] = 0xE9;
    std::int32_t relBack = (std::int32_t)(((std::uint8_t*)target + stolenBytes) - (jmpFrom + 5));
    std::memcpy(jmpFrom + 1, &relBack, sizeof(relBack));

    FlushInstructionCache(GetCurrentProcess(), tramp, stolenBytes + 5);
    return tramp;
}

// ------------------------------------------------------------
// Public API
// ------------------------------------------------------------
bool DamageHook::IsInstalled()
{
    return s_installed;
}

void DamageHook::Install()
{
    DebugLog::Write("=== Installing Damage Hook (CPed::InflictDamage) ===");

    if (s_installed) {
        DebugLog::Write("DamageHook already installed at 0x%08X", s_hookedAddr);
        return;
    }

    // IMPORTANT:
    // Your EXE may not match GTA3 1.0 addresses. We try a small list and log what sticks.
    // If none stick, we keep the system running (PedDeathTracker fallback heuristic still works),
    // but DirectDamageTracker will never fill.
    //
    // Known-ish candidates used by different GTA3 builds:
    // 1.0: 0x004B5B80 is commonly cited for CPed::InflictDamage
    // Others may differ (1.1/Steam/etc.) – you may need to add addresses for your build.
    const std::vector<unsigned int> candidates = {
        0x004B5B80,
        // Add more candidates here if you identify them for your EXE build.
    };

    for (auto addr : candidates) {
        if (TryInstallAtAddress(addr)) {
            s_installed = true;
            s_hookedAddr = addr;
            DebugLog::Write("SUCCESS: DamageHook installed at 0x%08X", addr);
            return;
        }
    }

    DebugLog::Write("FAILED: Could not install CPed::InflictDamage hook (no candidate address worked).");
    DebugLog::Write("PedDeathTracker will fall back to proximity + player-combat heuristics.");
}

// ------------------------------------------------------------
// Hook install attempt
// ------------------------------------------------------------
bool DamageHook::TryInstallAtAddress(unsigned int addr)
{
    void* target = reinterpret_cast<void*>(addr);

    // We will steal 5 bytes for a JMP. This is common but NOT instruction-safe in the general case.
    // In practice, many GTA functions have a prologue that is safe to steal (push ebp; mov ebp, esp; ...).
    constexpr std::size_t kStolen = 5;

    // Create trampoline BEFORE patching target
    void* tramp = MakeTrampoline(target, kStolen);
    if (!tramp) {
        DebugLog::Write("TryInstall: trampoline alloc failed at 0x%08X", addr);
        return false;
    }

    // Patch original -> hook
    if (!WriteRelJmp(target, (void*)&InflictDamageHook)) {
        DebugLog::Write("TryInstall: WriteRelJmp failed at 0x%08X", addr);
        return false;
    }

    s_original = reinterpret_cast<InflictDamage_t>(tramp);
    DebugLog::Write("TryInstall: trampoline at %p for 0x%08X", tramp, addr);
    return true;
}

// ------------------------------------------------------------
// Hook body
// ------------------------------------------------------------
bool __fastcall DamageHook::InflictDamageHook(
    CPed* self,
    void*,
    CEntity* damagedBy,
    eWeaponType weapon,
    float damage,
    ePedPieceTypes piece,
    unsigned char direction
)
{
    // Call original first to preserve game behavior.
    // (If you want to record "pre" state, you can move tracking before original.)
    bool result = false;
    if (s_original) {
        result = s_original(self, damagedBy, weapon, damage, piece, direction);
    }

    // Track only meaningful damage.
    if (!self || damage <= 0.0f || !damagedBy)
        return result;

    // We only reliably track *direct* player ped damage here
    // (Vehicle/explosion attribution can be added later if you want).
    CPed* playerPed = CWorld::Players[0].m_pPed;
    if (playerPed && damagedBy == playerPed) {
        DirectDamageTracker::RecordDamage(self, playerPed, damage);
    }

    return result;
}
