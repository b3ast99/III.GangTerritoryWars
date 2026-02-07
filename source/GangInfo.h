#pragma once
#include <vector>
#include <string>
#include "ePedType.h"
#include "eWeaponType.h"

struct GangInfo {
    ePedType gangType{};
    std::string displayName;
    std::vector<int> modelIds;
    std::vector<int> vehicleModelIds;
    eWeaponType defaultWeapon{};
    int blipColor{};
};

class GangManager {
public:
    static void Initialize();

    // NEW: call once after a save loads / player exists to retry name-based model resolution
    static void TryLateResolveModels();

    static const GangInfo* GetGangInfo(ePedType gangType);
    static int GetRandomModelId(ePedType gangType);
    static bool IsGangModelId(int modelId);
    static int GetRandomVehicleModelId(ePedType gangType);
    static bool IsGangVehicleModelId(int modelId);
    static const std::vector<int>& GetAmbientCivilianModelIds();
    static const char* GetGangName(ePedType gangType);
    static int GetGangBlipColor(ePedType gangType);
    static GangInfo s_gangs[3];
};
