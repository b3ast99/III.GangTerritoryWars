#pragma once
#include "plugin.h"
#include <cstdint>

extern "C" {
    extern volatile uint32_t g_PopAddPed_HitCount;
    extern volatile uint32_t g_PopAddPed_GangHitCount;
    extern volatile uint32_t g_PopAddPed_RewriteCount;
    extern volatile uint32_t g_PopAddPed_LastPedType;
    extern volatile uint32_t g_PopAddPed_LastModelArg;
    extern volatile uint32_t g_PopAddPed_LastOwnerGang;
}

class PopulationAddPedHook {
public:
    static void Install();
    static bool IsInstalled();

    static void SetEnabled(bool enabled);
    static bool IsEnabled();

    // optional: call each frame to emit a throttled status line
    static void DebugTick();

private:
    static bool TryInstallAtAddress(uint32_t addr);

    // GTA III population add func behaves like a global cdecl in practice
    using AddPed_t = CPed * (__cdecl*)(ePedType, unsigned int, const CVector&);

    static CPed* __cdecl AddPedHook(ePedType pedType, unsigned int modelIndexOrCopType, const CVector& coors);

private:
    static bool s_installed;
    static bool s_enabled;
    static uint32_t s_hookedAddr;
    static AddPed_t s_original;
};
