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
    static int GetRandomGangVehicle(ePedType gangType);
    static bool IsGangModelId(int modelId);
    static bool IsGangVehicleModel(int modelId);
    // Returns the PEDTYPE_GANG* for the given vehicle model, or -1 if not a gang vehicle.
    static int GetGangForVehicleModel(int modelId);
    static const std::vector<int>& GetAmbientCivilianModelIds();
    static const char* GetGangName(ePedType gangType);
    static int GetGangBlipColor(ePedType gangType);
    static GangInfo s_gangs[3];
};
