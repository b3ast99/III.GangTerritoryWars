#pragma once
#include "eWeaponType.h"
#include "PluginBase.h"
#include "ePedType.h"
#include <vector>
#include <string>

struct GangInfo {
    ePedType gangType;
    const char* displayName;
    std::vector<int> modelIds;          // Numeric model IDs (e.g., 14, 15 for Diablos)
    eWeaponType defaultWeapon;          // Default weapon for this gang
    int blipColor;                      // Blip colour (use values like BLIP_COLOUR_RED, etc.)
};

class GangManager {
public:
    static void Initialize();

    // Get gang info by ped type
    static const GangInfo* GetGangInfo(ePedType gangType);

    // Get gang info by territory owner
    static const GangInfo* GetGangInfoForTerritory(int territoryOwnerGang);

    // Get random model from gang's model list
    static int GetRandomModelId(ePedType gangType);

    static const char* GetGangName(ePedType gangType);
    static int GetGangBlipColor(ePedType gangType);


private:
    static GangInfo s_gangs[3]; // PEDTYPE_GANG1, GANG2, GANG3
};