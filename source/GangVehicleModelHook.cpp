// GangVehicleModelHook.cpp
#include "GangVehicleModelHook.h"

#include "DebugLog.h"
#include "TerritorySystem.h"
#include "GangInfo.h"
#include "CTimer.h"

#include <Windows.h>
#include <cstdint>
#include <cstring>
#include <cstdio>

// ------------------------------------------------------------
// Static state (matches GangVehicleModelHook.h)
// ------------------------------------------------------------
GangVehicleModelHook::SpawnContext GangVehicleModelHook::s_ctx[GangVehicleModelHook::kCtxCap]{};
int GangVehicleModelHook::s_ctxWrite = 0;

bool GangVehicleModelHook::s_installed = false;
bool GangVehicleModelHook::s_enabled = true;
GangVehicleModelHook::ChooseModel_t GangVehicleModelHook::s_original = nullptr;

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
namespace {

    static inline bool IsValidOwnerGang(int ownerGang) {
        return ownerGang >= (int)PEDTYPE_GANG1 && ownerGang <= (int)PEDTYPE_GANG3;
    }

    static bool IsAnyGangVehicleModel(int modelId) {
        if (modelId < 0) return false;
        for (int gi = 0; gi < 3; ++gi) {
            const GangInfo& g = GangManager::s_gangs[gi];
            for (int mid : g.vehicleModelIds) {
                if (mid >= 0 && mid == modelId) return true;
            }
        }
        return false;
    }

    static bool BytesMatch(const void* addr, const uint8_t* expected, size_t n) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(addr);
        for (size_t i = 0; i < n; ++i) {
            if (p[i] != expected[i]) return false;
        }
        return true;
    }

    static void DumpBytes(const char* label, const void* addr, size_t n) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(addr);
        char buf[256]{};
        char* w = buf;
        for (size_t i = 0; i < n; ++i) {
            w += std::snprintf(w, (size_t)(buf + sizeof(buf) - w), "%02X ", p[i]);
            if (w >= buf + sizeof(buf) - 4) break;
        }
        DebugLog::Write("%s @%p : %s", label, addr, buf);
    }

    static void* MakeTrampolineFixed(void* target, size_t stolenBytes) {
        uint8_t* src = reinterpret_cast<uint8_t*>(target);

        uint8_t* tramp = (uint8_t*)VirtualAlloc(nullptr, stolenBytes + 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!tramp) return nullptr;

        std::memcpy(tramp, src, stolenBytes);

        // jmp back to original after stolen bytes
        tramp[stolenBytes] = 0xE9;
        int32_t rel = (int32_t)((src + stolenBytes) - (tramp + stolenBytes + 5));
        std::memcpy(tramp + stolenBytes + 1, &rel, 4);

        FlushInstructionCache(GetCurrentProcess(), tramp, stolenBytes + 5);
        return tramp;
    }

    static bool PatchJmpAndNop(void* src, void* dst, size_t stolenBytes) {
        if (stolenBytes < 5) return false;

        DWORD oldProtect{};
        if (!VirtualProtect(src, stolenBytes, PAGE_EXECUTE_READWRITE, &oldProtect))
            return false;

        uint8_t* p = reinterpret_cast<uint8_t*>(src);

        // JMP rel32
        p[0] = 0xE9;
        int32_t rel = (int32_t)((uint8_t*)dst - ((uint8_t*)src + 5));
        std::memcpy(p + 1, &rel, 4);

        // NOP pad remaining bytes we "stole"
        for (size_t i = 5; i < stolenBytes; ++i) p[i] = 0x90;

        VirtualProtect(src, stolenBytes, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), src, stolenBytes);
        return true;
    }

} // namespace

// ------------------------------------------------------------
// Context push (matches header signature exactly)
// ------------------------------------------------------------
void GangVehicleModelHook::PushContext(int ownerGang, int vehicleModel, const CVector& pos, unsigned int nowMs) {
    SpawnContext& c = s_ctx[s_ctxWrite];
    c.ownerGang = ownerGang;
    c.territoryOwner = ownerGang;   // keep explicit, even if redundant
    c.vehicleModel = vehicleModel;
    c.pos = pos;

    // Tight window to reduce “context bleeding” into unrelated AddPed calls
    c.expiresMs = nowMs + 650;

    // Driver + a few passengers
    c.remaining = 5;

    s_ctxWrite = (s_ctxWrite + 1) % kCtxCap;
}

// ------------------------------------------------------------
// Install hook
// ------------------------------------------------------------
void GangVehicleModelHook::Install() {
    if (s_installed) {
        DebugLog::Write("GangVehicleModelHook already installed");
        return;
    }

    const uint32_t targetAddr = 0x00417EC0; // GTA III 1.0
    void* target = reinterpret_cast<void*>(targetAddr);

    // Expected prologue: mov eax,[esp+4]; mov ecx,[esp+8]
    // 8B 44 24 04  8B 4C 24 08
    const uint8_t expected[8] = { 0x8B, 0x44, 0x24, 0x04, 0x8B, 0x4C, 0x24, 0x08 };

    DumpBytes("ChooseModel bytes", target, 16);

    if (!BytesMatch(target, expected, 8)) {
        DebugLog::Write("GangVehicleModelHook: bytes mismatch at 0x%08X; NOT installing (wrong exe build/address).", targetAddr);
        return;
    }

    constexpr size_t stolen = 8;
    void* tramp = MakeTrampolineFixed(target, stolen);
    if (!tramp) {
        DebugLog::Write("GangVehicleModelHook: failed to alloc trampoline");
        return;
    }

    if (!PatchJmpAndNop(target, (void*)&Hook, stolen)) {
        DebugLog::Write("GangVehicleModelHook: failed to patch jmp");
        return;
    }

    s_original = reinterpret_cast<ChooseModel_t>(tramp);
    s_installed = true;

    DebugLog::Write("GangVehicleModelHook installed successfully at 0x%08X (stolen=%u)", targetAddr, (unsigned)stolen);
}

// ------------------------------------------------------------
// Hook body
// ------------------------------------------------------------
int __cdecl GangVehicleModelHook::Hook(CZoneInfo* zoneInfo, CVector* pos, int* outVehicleClass) {
    if (!s_original) return -1;
    if (!pos) return s_original(zoneInfo, pos, outVehicleClass);

    const int originalModel = s_original(zoneInfo, pos, outVehicleClass);

    if (!s_enabled) return originalModel;
    if (!TerritorySystem::HasRealTerritories()) return originalModel;

    // Only override if vanilla wanted a gang vehicle (keeps normal traffic normal)
    if (!IsAnyGangVehicleModel(originalModel)) return originalModel;

    const Territory* territory = TerritorySystem::GetTerritoryAtPoint(*pos);
    if (!territory) return originalModel;

    const int owner = territory->ownerGang;
    if (!IsValidOwnerGang(owner)) return originalModel;

    const int desiredModel = GangManager::GetRandomGangVehicle((ePedType)owner);
    if (desiredModel < 0) return originalModel;

    const unsigned int now = CTimer::m_snTimeInMilliseconds;

    // Push occupant context *for this spawn*, keyed to territory owner + final vehicle model.
    // Even if desired == original, we still want occupants to match the territory owner.
    PushContext(owner, desiredModel, *pos, now);

    // Throttle logs to avoid spam
    static unsigned int s_nextLogMs = 0;
    if (now >= s_nextLogMs) {
        s_nextLogMs = now + 1200;
        if (desiredModel != originalModel) {
            DebugLog::Write(
                "ChooseModel OVERRIDE (territory gang) -> terr=%s owner=%d pos(%.1f,%.1f,%.1f) original=%d -> desired=%d",
                territory->id.c_str(), owner, pos->x, pos->y, pos->z, originalModel, desiredModel
            );
        }
    }

    return (desiredModel >= 0) ? desiredModel : originalModel;
}
