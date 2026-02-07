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


static const std::vector<int> kAmbientCivilianModels = {
    30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
    50, 51, 52, 53, 55, 56, 57, 58, 59, 60,
    66, 67, 68, 69, 70, 71, 72, 73, 74, 75,
    76, 77, 78, 79, 80, 81, 82, 83
};

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


static bool TryResolveVehicleModelsInto(std::vector<int>& out, std::initializer_list<const char*> modelNames)
{
    out.clear();
    for (const char* name : modelNames) {
        const int modelId = ResolveModelIndexByName(name);
        if (modelId >= 0) {
            out.push_back(modelId);
        }
    }

    return !out.empty();
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
    SetFallbackModels(s_gangs[0].vehicleModelIds, { 90, 91 });

    // Triads
    s_gangs[1].gangType = PEDTYPE_GANG2;
    s_gangs[1].displayName = "Triads";
    s_gangs[1].defaultWeapon = WEAPONTYPE_UZI;
    s_gangs[1].blipColor = BLIP_COLOUR_GREEN;
    SetFallbackModels(s_gangs[1].modelIds, { 12, 13 });
    SetFallbackModels(s_gangs[1].vehicleModelIds, { 101, 102 });

    // Diablos
    s_gangs[2].gangType = PEDTYPE_GANG3;
    s_gangs[2].displayName = "Diablos";
    s_gangs[2].defaultWeapon = WEAPONTYPE_UZI;
    s_gangs[2].blipColor = BLIP_COLOUR_YELLOW;
    SetFallbackModels(s_gangs[2].modelIds, { 14, 15 });
    SetFallbackModels(s_gangs[2].vehicleModelIds, { 130, 131 });

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

    if (TryResolveVehicleModelsInto(s_gangs[0].vehicleModelIds, { "sentinel", "kuruma" })) {
        DebugLog::Write("GangInfo: resolved Mafia vehicles by name -> %d entries", (int)s_gangs[0].vehicleModelIds.size());
        anyResolved = true;
    }

    if (TryResolveVehicleModelsInto(s_gangs[1].vehicleModelIds, { "moonbeam", "pony" })) {
        DebugLog::Write("GangInfo: resolved Triad vehicles by name -> %d entries", (int)s_gangs[1].vehicleModelIds.size());
        anyResolved = true;
    }

    if (TryResolveVehicleModelsInto(s_gangs[2].vehicleModelIds, { "stallion", "manana" })) {
        DebugLog::Write("GangInfo: resolved Diablo vehicles by name -> %d entries", (int)s_gangs[2].vehicleModelIds.size());
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


int GangManager::GetRandomVehicleModelId(ePedType gangType)
{
    const GangInfo* info = GetGangInfo(gangType);
    if (!info || info->vehicleModelIds.empty())
        return -1;

    return info->vehicleModelIds[std::rand() % info->vehicleModelIds.size()];
}

bool GangManager::IsGangModelId(int modelId)
{
    for (const auto& g : s_gangs) {
        for (int mid : g.modelIds) {
            if (mid == modelId) {
                return true;
            }
        }
    }
    return false;
}

bool GangManager::IsGangVehicleModelId(int modelId)
{
    for (const auto& g : s_gangs) {
        for (int mid : g.vehicleModelIds) {
            if (mid == modelId) {
                return true;
            }
        }
    }
    return false;
}

const std::vector<int>& GangManager::GetAmbientCivilianModelIds()
{
    return kAmbientCivilianModels;
}
