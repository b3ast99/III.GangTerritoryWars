#include "GangInfo.h"
#include "plugin.h"
#include "CRadar.h"
#include <algorithm>  // for std::rand / shuffle if needed

// Initialize gang information
GangInfo GangManager::s_gangs[3];

void GangManager::Initialize() {
    // Mafia (Leone) - gang01/gang02
    s_gangs[0].gangType = PEDTYPE_GANG1;
    s_gangs[0].displayName = "Mafia";
    s_gangs[0].modelIds = { 10, 11 };  // gang01 = 7, gang02 = 8 (common community values; verify if needed)
    s_gangs[0].defaultWeapon = WEAPONTYPE_COLT45;
    s_gangs[0].blipColor = BLIP_COLOUR_RED;

    // Triads - gang03/gang04
    s_gangs[1].gangType = PEDTYPE_GANG2;
    s_gangs[1].displayName = "Triads";
    s_gangs[1].modelIds = { 12, 13 };  // Approximate; adjust if visual mismatch
    s_gangs[1].defaultWeapon = WEAPONTYPE_COLT45;
    s_gangs[1].blipColor = BLIP_COLOUR_YELLOW;

    // Diablos - gang05/gang06
    s_gangs[2].gangType = PEDTYPE_GANG3;
    s_gangs[2].displayName = "Diablos";
    s_gangs[2].modelIds = { 14, 15 };
    s_gangs[2].defaultWeapon = WEAPONTYPE_UZI;
    s_gangs[2].blipColor = BLIP_COLOUR_MAGENTA;
}



const GangInfo* GangManager::GetGangInfo(ePedType gangType) {
    for (int i = 0; i < 3; i++) {
        if (s_gangs[i].gangType == gangType) {
            return &s_gangs[i];
        }
    }
    return nullptr;
}

const GangInfo* GangManager::GetGangInfoForTerritory(int territoryOwnerGang) {
    ePedType gangType = static_cast<ePedType>(territoryOwnerGang);
    return GetGangInfo(gangType);
}

int GangManager::GetRandomModelId(ePedType gangType) {
    const GangInfo* info = GetGangInfo(gangType);
    if (!info || info->modelIds.empty()) {
        return -1;  // Fallback invalid
    }
    return info->modelIds[std::rand() % info->modelIds.size()];
}
const char* GangManager::GetGangName(ePedType gangType) {
    const GangInfo* info = GetGangInfo(gangType);
    return info ? info->displayName : "Unknown";
}

int GangManager::GetGangBlipColor(ePedType gangType) {
    const GangInfo* info = GetGangInfo(gangType);
    return info ? info->blipColor : BLIP_COLOUR_RED; // Default red
}