#pragma once
#include <cstdint>

#include "plugin.h"
#include "CVector.h"
#include "CZoneInfo.h"

class GangVehicleModelHook {
public:
    using ChooseModel_t = int(__cdecl*)(CZoneInfo* zoneInfo, CVector* pos, int* outVehicleClass);

    static void Install();
    static bool IsInstalled() { return s_installed; }

    static void SetEnabled(bool enabled) { s_enabled = enabled; }
    static bool IsEnabled() { return s_enabled; }

    // Spawn context entries for matching occupant AddPed calls.
    struct SpawnContext {
        unsigned int expiresMs = 0;
        int ownerGang = -1;           // PEDTYPE_GANG1..3
        int territoryOwner = -1;      // owner at spawn time (same as ownerGang, but explicit)
        int vehicleModel = -1;        // desired gang vehicle model
        CVector pos{ 0,0,0 };
        int remaining = 0;
    };


    // Ring buffer
    static constexpr int kCtxCap = 8;
    static SpawnContext s_ctx[kCtxCap];
    static int s_ctxWrite;

    // Push a new context (called by ChooseModel hook)
    static void PushContext(int ownerGang, int vehicleModel, const CVector& pos, unsigned int nowMs);

    // Consume a pending vehicle spawn context for AddPed occupant rewrites.
    static bool TryConsumeOwnerGangForSpawn(const CVector& pedSpawnPos, int& outOwnerGang);

private:
    static int __cdecl Hook(CZoneInfo* zoneInfo, CVector* pos, int* outVehicleClass);

    static bool s_installed;
    static bool s_enabled;
    static ChooseModel_t s_original;
};
