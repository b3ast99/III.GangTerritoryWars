#include "GangInfo.h"
#include "plugin.h"
#include "CRadar.h"

#include "CModelInfo.h"
#include "CBaseModelInfo.h"
#include "CStreaming.h"

#include "DebugLog.h"

#include <cstdlib>
#include <initializer_list>

GangInfo GangManager::s_gangs[3];

static int ResolveModelIndexByName(const char* modelName)
{
    int idx = -1;
    CBaseModelInfo* mi = CModelInfo::GetModelInfo(modelName, &idx);
    if (!mi || idx < 0) return -1;
    return idx;
}

static void SetFallbackModels(std::vector<int>& out, std::initializer_list<int> fallback)
{
    out.assign(fallback.begin(), fallback.end());
}

static bool TryResolveInto(std::vector<int>& out, const char* a, const char* b)
{
    const int ia = ResolveModelIndexByName(a);
    const int ib = ResolveModelIndexByName(b);

    if (ia < 0 || ib < 0) return false;

    out.clear();
    out.push_back(ia);
    if (ib != ia) out.push_back(ib);
    return true;
}

void GangManager::Initialize()
{
    DebugLog::Write("GangManager::Initialize");

    // Mafia (Leone)
    s_gangs[0].gangType = PEDTYPE_GANG1;
    s_gangs[0].displayName = "Mafia";
    s_gangs[0].defaultWeapon = WEAPONTYPE_COLT45;
    s_gangs[0].blipColor = BLIP_COLOUR_RED;
    SetFallbackModels(s_gangs[0].modelIds, { 10, 11 });

    // Triads
    s_gangs[1].gangType = PEDTYPE_GANG2;
    s_gangs[1].displayName = "Triads";
    s_gangs[1].defaultWeapon = WEAPONTYPE_UZI;
    s_gangs[1].blipColor = BLIP_COLOUR_GREEN;
    SetFallbackModels(s_gangs[1].modelIds, { 12, 13 });

    // Diablos
    s_gangs[2].gangType = PEDTYPE_GANG3;
    s_gangs[2].displayName = "Diablos";
    s_gangs[2].defaultWeapon = WEAPONTYPE_UZI;
    s_gangs[2].blipColor = BLIP_COLOUR_YELLOW;
    SetFallbackModels(s_gangs[2].modelIds, { 14, 15 });

    // Try once immediately (may fail early; that’s fine)
    TryLateResolveModels();

    DebugLog::Write("GangManager::Initialize done");
}

void GangManager::TryLateResolveModels()
{
    // Don’t spam: resolve at most once successfully, and retry a few times early-game.
    static bool s_done = false;
    static int s_attempts = 0;

    if (s_done) return;
    if (s_attempts >= 20) return; // try for ~20 frames after load; adjust if needed
    s_attempts++;

    bool anyResolved = false;

    // If these names are wrong for your dataset, we’ll learn that from the modelIndex logs next.
    if (TryResolveInto(s_gangs[0].modelIds, "gang01", "gang02")) {
        DebugLog::Write("GangInfo: resolved Mafia by name -> %d,%d",
            s_gangs[0].modelIds[0], s_gangs[0].modelIds.size() > 1 ? s_gangs[0].modelIds[1] : -1);
        anyResolved = true;
    }

    if (TryResolveInto(s_gangs[1].modelIds, "gang03", "gang04")) {
        DebugLog::Write("GangInfo: resolved Triads by name -> %d,%d",
            s_gangs[1].modelIds[0], s_gangs[1].modelIds.size() > 1 ? s_gangs[1].modelIds[1] : -1);
        anyResolved = true;
    }

    if (TryResolveInto(s_gangs[2].modelIds, "gang05", "gang06")) {
        DebugLog::Write("GangInfo: resolved Diablos by name -> %d,%d",
            s_gangs[2].modelIds[0], s_gangs[2].modelIds.size() > 1 ? s_gangs[2].modelIds[1] : -1);
        anyResolved = true;
    }

    // If *any* resolved, we’re happy; if none resolved after 20 attempts we stop trying.
    if (anyResolved) {
        // No duplicate requests - preload already happened in Main.cpp
        // Comment out or remove these lines:
        // for (const auto& g : s_gangs) { ... RequestModel ... }
        // CStreaming::LoadAllRequestedModels(true);

        s_done = true;
        DebugLog::Write("GangInfo: late resolve SUCCESS (models already preloaded)");
    }
    else if (s_attempts == 1) {
        DebugLog::Write("GangInfo: late resolve attempt started (will retry)");
    }
    else if (s_attempts == 20) {
        DebugLog::Write("GangInfo: late resolve FAILED after 20 attempts (keeping fallback indices)");
    }
}

const GangInfo* GangManager::GetGangInfo(ePedType gangType)
{
    for (const GangInfo& g : s_gangs) {
        if (g.gangType == gangType)
            return &g;
    }
    return nullptr;
}

int GangManager::GetRandomModelId(ePedType gangType)
{
    const GangInfo* info = GetGangInfo(gangType);
    if (!info || info->modelIds.empty())
        return -1;

    return info->modelIds[std::rand() % info->modelIds.size()];
}

const char* GangManager::GetGangName(ePedType gangType)
{
    const GangInfo* info = GetGangInfo(gangType);
    return info ? info->displayName.c_str() : "Unknown";
}

int GangManager::GetGangBlipColor(ePedType gangType)
{
    const GangInfo* info = GetGangInfo(gangType);
    return info ? info->blipColor : BLIP_COLOUR_RED;
}
