#include "AmbientGangVehicleSpawner.h"

#include "plugin.h"
#include "CTimer.h"
#include "CWorld.h"
#include "CStreaming.h"
#include "CVehicle.h"
#include "CPools.h"
#include "eScriptCommands.h"
#include "scripting.h"

#include "TerritorySystem.h"
#include "GangInfo.h"
#include "DebugLog.h"

#include <cmath>
#include <cstdint>

namespace {
    static constexpr unsigned int VEHICLE_INJECT_INTERVAL_MS = 12000;
    static constexpr float VEHICLE_INJECT_RADIUS_MIN = 25.0f;
    static constexpr float VEHICLE_INJECT_RADIUS_MAX = 55.0f;
    static constexpr float VEHICLE_DENSITY_RADIUS = 80.0f;
    static constexpr int MAX_GANG_VEHICLES_IN_AREA = 3;

    static constexpr unsigned int VANILLA_REWRITE_SCAN_INTERVAL_MS = 2500;
    static constexpr float VANILLA_REWRITE_RADIUS = 140.0f;

    static bool IsModelLoaded(int modelIndex) {
        if (modelIndex < 0 || modelIndex >= 20000) return false;
        return CStreaming::ms_aInfoForModel[modelIndex].m_nLoadState == LOADSTATE_LOADED;
    }

    static bool IsGangVehicleForOwner(int modelIndex, ePedType ownerGang) {
        const GangInfo* info = GangManager::GetGangInfo(ownerGang);
        if (!info) return false;

        for (int id : info->vehicleModelIds) {
            if (id == modelIndex) {
                return true;
            }
        }

        return false;
    }

    static int CountVehiclesInRadius(const CVector& center, float radius) {
        CEntity* nearby[48]{};
        short numNearby = 0;

        CWorld::FindObjectsInRange(
            center,
            radius,
            true,
            &numNearby,
            48,
            nearby,
            false,
            true,
            false,
            false,
            false
        );

        int count = 0;
        for (short i = 0; i < numNearby; ++i) {
            if (nearby[i] && nearby[i]->m_nType == ENTITY_TYPE_VEHICLE) {
                ++count;
            }
        }

        return count;
    }

    static bool ReplaceVehicleModelInPlace(CVehicle* vehicle, int desiredModel, const Territory& territory, int ownerGang) {
        if (!vehicle || desiredModel < 0 || !IsModelLoaded(desiredModel)) {
            return false;
        }

        const CVector pos = vehicle->GetPosition();
        const float heading = vehicle->GetHeading();

        int oldHandle = CPools::GetVehicleRef(vehicle);
        if (oldHandle < 0) {
            return false;
        }

        int newHandle = -1;
        plugin::scripting::CallCommandById(static_cast<unsigned short>(eScriptCommands::COMMAND_CREATE_CAR), desiredModel, pos.x, pos.y, pos.z, &newHandle);
        if (newHandle < 0) {
            return false;
        }

        plugin::scripting::CallCommandById(static_cast<unsigned short>(eScriptCommands::COMMAND_SET_CAR_HEADING), newHandle, heading);
        plugin::scripting::CallCommandById(static_cast<unsigned short>(eScriptCommands::COMMAND_DELETE_CAR), oldHandle);

        DebugLog::Write("AmbientVehicle REWRITE: terr=%s owner=%d oldModel=%d newModel=%d at %.1f,%.1f,%.1f",
            territory.id.c_str(), ownerGang, vehicle->m_nModelIndex, desiredModel, pos.x, pos.y, pos.z);

        return true;
    }

    static void RewriteVanillaGangVehiclesNearPlayer(const Territory& territory, int ownerGang, const CVector& playerPos) {
        CEntity* nearby[48]{};
        short numNearby = 0;

        CWorld::FindObjectsInRange(
            playerPos,
            VANILLA_REWRITE_RADIUS,
            true,
            &numNearby,
            48,
            nearby,
            false,
            true,
            false,
            false,
            false
        );

        int rewrites = 0;
        for (short i = 0; i < numNearby; ++i) {
            CEntity* ent = nearby[i];
            if (!ent || ent->m_nType != ENTITY_TYPE_VEHICLE) {
                continue;
            }

            CVehicle* vehicle = static_cast<CVehicle*>(ent);
            const int model = vehicle->m_nModelIndex;

            if (!GangManager::IsGangVehicleModelId(model)) {
                continue;
            }

            if (IsGangVehicleForOwner(model, static_cast<ePedType>(ownerGang))) {
                continue;
            }

            const int desiredModel = GangManager::GetRandomVehicleModelId(static_cast<ePedType>(ownerGang));
            if (desiredModel < 0 || desiredModel == model) {
                continue;
            }

            if (ReplaceVehicleModelInPlace(vehicle, desiredModel, territory, ownerGang)) {
                ++rewrites;
            }
        }

        if (rewrites > 0) {
            DebugLog::Write("AmbientVehicle: rewrote %d vanilla gang vehicles in terr=%s (owner=%d)",
                rewrites, territory.id.c_str(), ownerGang);
        }
    }

    static bool TryCreateAmbientGangVehicle(const Territory& territory, int ownerGang, const CVector& playerPos) {
        if (CountVehiclesInRadius(playerPos, VEHICLE_DENSITY_RADIUS) >= MAX_GANG_VEHICLES_IN_AREA) {
            return false;
        }

        const float angle = plugin::RandomNumberInRange(0.0f, 6.283185307f);
        const float dist = plugin::RandomNumberInRange(VEHICLE_INJECT_RADIUS_MIN, VEHICLE_INJECT_RADIUS_MAX);

        CVector spawnPos = playerPos;
        spawnPos.x += std::cos(angle) * dist;
        spawnPos.y += std::sin(angle) * dist;

        if (spawnPos.x < territory.minX) spawnPos.x = territory.minX + 2.0f;
        if (spawnPos.x > territory.maxX) spawnPos.x = territory.maxX - 2.0f;
        if (spawnPos.y < territory.minY) spawnPos.y = territory.minY + 2.0f;
        if (spawnPos.y > territory.maxY) spawnPos.y = territory.maxY - 2.0f;

        bool foundGround = false;
        float groundZ = CWorld::FindGroundZFor3DCoord(spawnPos.x, spawnPos.y, playerPos.z + 60.0f, &foundGround);
        if (!foundGround) {
            return false;
        }

        spawnPos.z = groundZ + 1.0f;

        const int vehicleModel = GangManager::GetRandomVehicleModelId(static_cast<ePedType>(ownerGang));
        if (vehicleModel < 0 || !IsModelLoaded(vehicleModel)) {
            return false;
        }

        int vehicleHandle = -1;
        plugin::scripting::CallCommandById(static_cast<unsigned short>(eScriptCommands::COMMAND_CREATE_CAR), vehicleModel, spawnPos.x, spawnPos.y, spawnPos.z, &vehicleHandle);
        if (vehicleHandle < 0) {
            return false;
        }

        const float heading = plugin::RandomNumberInRange(0.0f, 359.0f);
        plugin::scripting::CallCommandById(static_cast<unsigned short>(eScriptCommands::COMMAND_SET_CAR_HEADING), vehicleHandle, heading);

        DebugLog::Write("AmbientVehicle: spawned gang vehicle terr=%s gang=%d model=%d at %.1f,%.1f,%.1f",
            territory.id.c_str(), ownerGang, vehicleModel, spawnPos.x, spawnPos.y, spawnPos.z);

        return true;
    }
}

void AmbientGangVehicleSpawner::Process() {
    CPlayerPed* player = CWorld::Players[0].m_pPed;
    if (!player) {
        return;
    }

    const CVector playerPos = player->GetPosition();
    const Territory* territory = TerritorySystem::GetTerritoryAtPoint(playerPos);
    if (!territory) {
        return;
    }

    const int ownerGang = territory->ownerGang;
    if (ownerGang < static_cast<int>(PEDTYPE_GANG1) || ownerGang > static_cast<int>(PEDTYPE_GANG9)) {
        return;
    }

    static unsigned int s_nextRewriteScanMs = 0;
    const unsigned int now = CTimer::m_snTimeInMilliseconds;
    if (now >= s_nextRewriteScanMs) {
        RewriteVanillaGangVehiclesNearPlayer(*territory, ownerGang, playerPos);
        s_nextRewriteScanMs = now + VANILLA_REWRITE_SCAN_INTERVAL_MS;
    }

    static unsigned int s_nextInjectMs = 0;
    if (now < s_nextInjectMs) {
        return;
    }

    s_nextInjectMs = now + VEHICLE_INJECT_INTERVAL_MS;

    if (!TryCreateAmbientGangVehicle(*territory, ownerGang, playerPos)) {
        DebugLog::Write("AmbientVehicle: skipped terr=%s gang=%d", territory->id.c_str(), ownerGang);
    }
}
