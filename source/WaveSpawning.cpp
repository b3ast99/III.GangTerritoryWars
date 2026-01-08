// WaveSpawning.cpp
#include "WaveSpawning.h"
#include "WaveConfig.h"
#include "GangInfo.h"
#include "TerritorySystem.h"
#include "DebugLog.h"
#include "CWorld.h"
#include "CStreaming.h"
#include "CPopulation.h"
#include "CPools.h"
#include "CModelInfo.h"
#include "CColPoint.h"
#include "CCollision.h"
#include "plugin.h"
#include <cmath>
#include <algorithm>

namespace WaveSpawning {
    // Utility functions
    namespace {
        float Rand01() {
            return plugin::RandomNumberInRange(0.0f, 1.0f);
        }

        float RandRange(float a, float b) {
            return plugin::RandomNumberInRange(a, b);
        }

        int RandRange(int a, int b) {
            return plugin::RandomNumberInRange(a, b);
        }

        float Dist2D(const CVector& a, const CVector& b) {
            float dx = a.x - b.x;
            float dy = a.y - b.y;
            return std::sqrt(dx * dx + dy * dy);
        }

        float Dist3D(const CVector& a, const CVector& b) {
            float dx = a.x - b.x;
            float dy = a.y - b.y;
            float dz = a.z - b.z;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }

        int GetHandle(CPed* ped) {
            return ped ? CPools::GetPedRef(ped) : -1;
        }
    }

    // Core spawning implementation
    WaveSpawnPlan PlanWaveSpawn(
        ePedType gangType,
        const Territory* territory,
        int waveIndex,
        int targetCount) {

        WaveSpawnPlan plan;
        CPlayerPed* player = CWorld::Players[0].m_pPed;
        if (!player) return plan;

        // Determine number of clusters
        int numClusters = CalculateClusterCount(targetCount);

        // Calculate enemies per cluster
        plan.clusterSizes = CalculateClusterSizes(targetCount, numClusters);

        DebugLog::Write("Wave %d: Planning %d enemies in %d clusters",
            waveIndex, targetCount, numClusters);

        // Find cluster centers
        plan.clusterCenters = FindClusterCenters(
            territory, player->GetPosition(), numClusters, waveIndex);

        return plan;
    }

    // Keep the old SpawnWaveEnemies for backward compatibility if needed
    std::vector<SpawnResult> SpawnWaveEnemies(
        ePedType gangType,
        const Territory* territory,
        int waveIndex,
        int targetCount) {

        std::vector<SpawnResult> allResults;
        auto plan = PlanWaveSpawn(gangType, territory, waveIndex, targetCount);

        // Spawn all clusters immediately (original behavior)
        for (size_t i = 0; i < plan.clusterCenters.size(); i++) {
            auto clusterResults = SpawnSingleClusterEnemies(
                gangType, territory, waveIndex,
                plan.clusterCenters[i], plan.clusterSizes[i]);

            allResults.insert(allResults.end(),
                clusterResults.begin(), clusterResults.end());
        }

        DebugLog::Write("Spawned %d/%d enemies total", allResults.size(), targetCount);
        return allResults;
    }

    int CalculateClusterCount(int targetCount) {
        if (targetCount >= 8) return 3;
        if (targetCount >= 5) return 2;
        return 1;
    }

    std::vector<int> CalculateClusterSizes(int targetCount, int numClusters) {
        std::vector<int> clusterSizes(numClusters, targetCount / numClusters);
        int remainder = targetCount % numClusters;
        for (int i = 0; i < remainder; i++) {
            clusterSizes[i]++;
        }
        return clusterSizes;
    }

    std::vector<CVector> FindClusterCenters(
        const Territory* territory,
        const CVector& playerPos,
        int numClusters,
        int waveIndex) {

        std::vector<CVector> clusterCenters;
        std::vector<CVector> dummyPositions;

        // Get player ped for heading calculation
        CPlayerPed* player = CWorld::Players[0].m_pPed;

        // Find first cluster center
        CVector firstCenter;
        if (FindStrategicSpawnPosition(firstCenter, territory, playerPos,
            dummyPositions, waveIndex)) {
            clusterCenters.push_back(firstCenter);
            DebugLog::Write("Cluster 1 center at %.1f, %.1f", firstCenter.x, firstCenter.y);
        }
        else {
            firstCenter = CreateFallbackClusterCenter(playerPos, territory);
            clusterCenters.push_back(firstCenter);
            DebugLog::Write("Cluster 1 fallback at %.1f, %.1f", firstCenter.x, firstCenter.y);
        }

        // Find additional cluster centers
        for (int i = 1; i < numClusters; i++) {
            CVector center = FindAdditionalClusterCenter(
                territory, playerPos, waveIndex, clusterCenters, i);
            clusterCenters.push_back(center);
        }

        return clusterCenters;
    }

    CVector CreateFallbackClusterCenter(const CVector& playerPos, const Territory* territory) {
        float angle = RandRange(0.0f, 6.283185f);
        CVector center = playerPos;
        center.x += 50.0f * cos(angle);
        center.y += 50.0f * sin(angle);

        if (territory) {
            center.x = std::clamp(center.x, territory->minX + 10.0f, territory->maxX - 10.0f);
            center.y = std::clamp(center.y, territory->minY + 10.0f, territory->maxY - 10.0f);
        }

        return center;
    }

    CVector FindAdditionalClusterCenter(
        const Territory* territory,
        const CVector& playerPos,
        int waveIndex,
        const std::vector<CVector>& existingCenters,
        int clusterIndex) {

        std::vector<CVector> dummyPositions;
        CVector center;

        for (int attempts = 0; attempts < 25; attempts++) {
            if (FindStrategicSpawnPosition(center, territory, playerPos,
                dummyPositions, waveIndex)) {

                bool tooClose = false;
                for (const auto& existing : existingCenters) {
                    if (Dist2D(center, existing) < 40.0f) {
                        tooClose = true;
                        break;
                    }
                }

                if (!tooClose) {
                    DebugLog::Write("Cluster %d center at %.1f, %.1f",
                        clusterIndex + 1, center.x, center.y);
                    return center;
                }
            }
        }

        // If no valid position found, create one
        return CreateForcedClusterCenter(existingCenters[0], existingCenters, territory, clusterIndex);
    }

    CVector CreateForcedClusterCenter(
        const CVector& firstCenter,
        const std::vector<CVector>& existingCenters,
        const Territory* territory,
        int clusterIndex) {

        for (int attempts = 0; attempts < 10; attempts++) {
            float angle = RandRange(0.0f, 6.283185f);
            CVector newCenter = firstCenter;
            newCenter.x += 60.0f * cos(angle);
            newCenter.y += 60.0f * sin(angle);

            // Ensure minimum distance from other centers
            bool valid = true;
            for (const auto& existing : existingCenters) {
                if (Dist2D(newCenter, existing) < 40.0f) {
                    valid = false;
                    break;
                }
            }

            if (valid) {
                // Ensure it's in territory
                if (territory) {
                    newCenter.x = std::clamp(newCenter.x, territory->minX + 10.0f, territory->maxX - 10.0f);
                    newCenter.y = std::clamp(newCenter.y, territory->minY + 10.0f, territory->maxY - 10.0f);
                }

                DebugLog::Write("Cluster %d forced at %.1f, %.1f",
                    clusterIndex + 1, newCenter.x, newCenter.y);
                return newCenter;
            }
        }

        // Last resort: just offset from first center
        CVector lastResort = firstCenter;
        lastResort.x += 60.0f;
        lastResort.y += 60.0f;
        return lastResort;
    }

    std::vector<SpawnResult> SpawnClusterEnemies(
        ePedType gangType,
        const Territory* territory,
        int waveIndex,
        CPlayerPed* player,
        const CVector& clusterCenter,
        int enemiesToSpawn,
        int& totalSpawnedCounter) {

        std::vector<SpawnResult> results;

        for (int i = 0; i < enemiesToSpawn; i++) {
            // Get enemy model
            int modelId = GetEnemyModelId(gangType);
            if (modelId < 0) continue;

            // Calculate spawn position
            CVector spawnPos = CalculateSpawnPosition(clusterCenter, territory);

            // Spawn enemy
            CPed* ped = SpawnSingleEnemy(gangType, modelId, spawnPos);
            if (!ped) continue;

            // Configure enemy
            ConfigureEnemyPed(ped, gangType, waveIndex, player);

            // Add to results
            results.push_back(CreateSpawnResult(ped, spawnPos));
            totalSpawnedCounter++;

            DebugLog::Write("Spawned enemy %d in cluster at %.1f, %.1f",
                totalSpawnedCounter, spawnPos.x, spawnPos.y);
        }

        return results;
    }

    std::vector<SpawnResult> SpawnSingleClusterEnemies(
        ePedType gangType,
        const Territory* territory,
        int waveIndex,
        const CVector& clusterCenter,
        int enemiesInCluster) {

        std::vector<SpawnResult> results;
        CPlayerPed* player = CWorld::Players[0].m_pPed;
        if (!player) return results;

        int dummyCounter = 0; // Not used, but needed for function signature
        return SpawnClusterEnemies(gangType, territory, waveIndex, player,
            clusterCenter, enemiesInCluster, dummyCounter);
    }

    int GetEnemyModelId(ePedType gangType) {
        int modelId = GangManager::GetRandomModelId(gangType);
        if (modelId >= 0) return modelId;

        const GangInfo* gangInfo = GangManager::GetGangInfo(gangType);
        if (gangInfo && !gangInfo->modelIds.empty()) {
            return gangInfo->modelIds[0];
        }

        return -1;
    }

    // Add this function to detect if position is on a roof or elevated structure
    bool IsPositionOnRoof(const CVector& pos) {
        // Check if there's a ceiling/roof above (within 5 meters)
        CColPoint colPoint;
        CEntity* colEntity = nullptr;

        CVector rayStart = pos;
        rayStart.z += 1.0f;  // Start just above ground
        CVector rayEnd = rayStart;
        rayEnd.z += 20.0f;   // Check 20 meters up

        if (CWorld::ProcessVerticalLine(rayStart, rayEnd.z, colPoint, colEntity,
            true, false, false, false, true, false, nullptr)) {
            // If we hit something within 10 meters above, it's probably a roof/overpass
            if (colPoint.m_vecPoint.z - pos.z < 10.0f) {
                return true;
            }
        }

        // Additional check: if position is more than 15m above sea level in city areas
        // (adjust this threshold based on your map)
        if (pos.z > 20.0f) {
            // Check if we're in a city area (not mountains)
            // You might want to add territory-based checks here
            return true;
        }

        return false;
    }

    CVector CalculateSpawnPosition(const CVector& clusterCenter, const Territory* territory) {
        CVector spawnPos = clusterCenter;

        // Try multiple positions to find one at reasonable elevation
        for (int attempt = 0; attempt < 5; attempt++) {
            float angle = RandRange(0.0f, 6.283185f);
            float distance = RandRange(3.0f, 12.0f);

            spawnPos.x = clusterCenter.x + distance * cos(angle);
            spawnPos.y = clusterCenter.y + distance * sin(angle);

            // Find ground with elevation limit
            float groundZ;
            if (FindGroundZWithElevationLimit(spawnPos.x, spawnPos.y, spawnPos.z, groundZ, 10.0f)) {
                spawnPos.z = groundZ + 1.0f;

                // Additional check: make sure it's not on a rooftop or elevated structure
                if (!IsPositionOnRoof(spawnPos)) {
                    return spawnPos;
                }
            }
        }

        // Fallback: use original ground finding without elevation check
        float groundZ;
        if (!FindGroundZForCoord(spawnPos.x, spawnPos.y, spawnPos.z, groundZ)) {
            groundZ = clusterCenter.z;
        }
        spawnPos.z = groundZ + 1.0f;

        return spawnPos;
    }

    CPed* SpawnSingleEnemy(ePedType gangType, int modelId, const CVector& position) {
        return CPopulation::AddPed(gangType, modelId, position);
    }

    SpawnResult CreateSpawnResult(CPed* ped, const CVector& position) {
        SpawnResult result;
        result.ped = ped;
        result.pedHandle = GetHandle(ped);
        result.position = position;
        return result;
    }

    bool IsPositionActuallyWalkable(const CVector& pos) {
        const float TEST_DISTANCE = 8.0f;  // Test 8m in cardinal directions
        const float MAX_Z_VARIANCE = 2.5f; // Ground shouldn't vary more than 2.5m

        // Test positions in 4 cardinal directions
        CVector testPoints[4] = {
            {pos.x + TEST_DISTANCE, pos.y, pos.z},  // East
            {pos.x - TEST_DISTANCE, pos.y, pos.z},  // West
            {pos.x, pos.y + TEST_DISTANCE, pos.z},  // North
            {pos.x, pos.y - TEST_DISTANCE, pos.z}   // South
        };

        int validDirections = 0;

        for (const auto& testPoint : testPoints) {
            float testGroundZ;
            if (!FindGroundZForCoord(testPoint.x, testPoint.y, testPoint.z, testGroundZ)) {
                continue; // No ground found in this direction
            }

            // Check if ground height is similar (indicates continuous walkable surface)
            if (std::abs(testGroundZ - pos.z) <= MAX_Z_VARIANCE) {
                // Also verify no obstacles blocking the path
                CColPoint colPoint;
                CEntity* colEntity = nullptr;

                CVector from = pos;
                from.z += 0.5f; // Chest height
                CVector to = testPoint;
                to.z = testGroundZ + 0.5f;

                // If line is clear, this direction is walkable
                if (!CWorld::ProcessLineOfSight(from, to, colPoint, colEntity,
                    true, true, false, false, false, false, false)) {
                    validDirections++;
                }
            }
        }

        // Position is walkable if at least 2 directions are clear
        // (allows corners/edges but rejects isolated spots)
        return validDirections >= 2;
    }

    // Position finding (copied from your original)
    bool FindStrategicSpawnPosition(
        CVector& outPos,
        const Territory* terr,
        const CVector& playerPos,
        const std::vector<CVector>& existingSpawns,
        int waveIndex) {

        const float MIN_DIST_FROM_PLAYER = 35.0f;
        const float MAX_DIST_FROM_PLAYER = 65.0f;
        const float MIN_SPAWN_SEPARATION = 10.0f;
        const float MAX_ELEVATION_DIFF = 10.0f;  // NEW: Max height difference from player

        // Get player for heading
        CPlayerPed* player = CWorld::Players[0].m_pPed;
        float playerHeading = 0.0f;
        if (player) {
            playerHeading = atan2(player->m_matrix.up.y, player->m_matrix.up.x);
        }

        struct SpawnQuadrant {
            float angleOffset;
            float distanceMin;
            float distanceMax;
            float preference;
        };

        std::vector<SpawnQuadrant> quadrants = {
            {3.14159f, MIN_DIST_FROM_PLAYER, MAX_DIST_FROM_PLAYER, 1.0f},
            {2.35619f, MIN_DIST_FROM_PLAYER, MAX_DIST_FROM_PLAYER * 0.8f, 0.7f},
            {-2.35619f, MIN_DIST_FROM_PLAYER, MAX_DIST_FROM_PLAYER * 0.8f, 0.7f},
            {0.785398f, MIN_DIST_FROM_PLAYER * 1.2f, MAX_DIST_FROM_PLAYER * 0.9f, 0.5f},
            {-0.785398f, MIN_DIST_FROM_PLAYER * 1.2f, MAX_DIST_FROM_PLAYER * 0.9f, 0.5f},
            {0.0f, MIN_DIST_FROM_PLAYER * 1.5f, MAX_DIST_FROM_PLAYER * 0.7f, 0.3f}
        };

        for (const auto& quadrant : quadrants) {
            if (Rand01() > quadrant.preference) continue;

            for (int attempts = 0; attempts < 12; ++attempts) {
                float angle = playerHeading + quadrant.angleOffset;
                angle += RandRange(-0.5f, 0.5f);
                float distance = RandRange(quadrant.distanceMin, quadrant.distanceMax);

                CVector candidate;
                candidate.x = playerPos.x + distance * cos(angle);
                candidate.y = playerPos.y + distance * sin(angle);
                candidate.z = playerPos.z;  // Use player's Z initially

                if (terr) {
                    if (candidate.x < terr->minX || candidate.x > terr->maxX ||
                        candidate.y < terr->minY || candidate.y > terr->maxY) {
                        continue;
                    }
                }

                float groundZ;
                if (!FindGroundZForCoord(candidate.x, candidate.y, candidate.z, groundZ)) {
                    continue;
                }

                // FIX: Check elevation difference
                float elevationDiff = std::abs(groundZ - playerPos.z);
                if (elevationDiff > MAX_ELEVATION_DIFF) {
                    continue;  // Too high or too low, skip this position
                }

                candidate.z = groundZ + 1.0f;

                if (!IsPositionActuallyWalkable(candidate)) {
                    continue; // Skip isolated/inaccessible positions
                }

                // Check for collisions at spawn position
                if (CWorld::TestSphereAgainstWorld(candidate, 1.0f, nullptr,
                    true, true, true, true, true, true)) {
                    continue;
                }

                bool isVisible = IsVisibleFromPlayer(candidate, playerPos);
                if (isVisible && waveIndex == 0 && Rand01() < 0.7f) {
                    continue;
                }

                bool tooClose = false;
                for (const auto& existing : existingSpawns) {
                    if (Dist2D(candidate, existing) < MIN_SPAWN_SEPARATION) {
                        tooClose = true;
                        break;
                    }
                }
                if (tooClose) continue;

                outPos = candidate;
                return true;
            }
        }

        // Fallback - ALSO FIX ELEVATION CHECK HERE
        for (int attempts = 0; attempts < 25; ++attempts) {
            float angle = RandRange(0.0f, 6.283185f);
            float distance = RandRange(MIN_DIST_FROM_PLAYER, MAX_DIST_FROM_PLAYER);

            CVector candidate;
            candidate.x = playerPos.x + distance * cos(angle);
            candidate.y = playerPos.y + distance * sin(angle);
            candidate.z = playerPos.z;

            if (terr) {
                if (candidate.x < terr->minX || candidate.x > terr->maxX ||
                    candidate.y < terr->minY || candidate.y > terr->maxY) {
                    continue;
                }
            }

            float groundZ = 0.0f;
            if (!FindGroundZForCoord(candidate.x, candidate.y, candidate.z, groundZ)) {
                continue;
            }

            // FIX: Check elevation difference in fallback too
            float elevationDiff = std::abs(groundZ - playerPos.z);
            if (elevationDiff > MAX_ELEVATION_DIFF) {
                continue;  // Too high or too low
            }

            candidate.z = groundZ + 1.0f;

            if (!CWorld::TestSphereAgainstWorld(candidate, 1.0f, nullptr,
                true, true, true, true, true, true)) {
                outPos = candidate;
                return true;
            }
        }

        return false;
    }

    // Environment checks (copied from your original)
    bool IsPositionInWater(const CVector& pos) {
        return pos.z < 3.0f;
    }

    bool FindGroundZForCoord(float x, float y, float z, float& groundZ) {
        bool foundGround = false;
        groundZ = CWorld::FindGroundZFor3DCoord(x, y, z + 50.0f, &foundGround);

        if (!foundGround) {
            CColPoint colPoint;
            CEntity* colEntity = nullptr;

            CVector rayStart(x, y, z + 50.0f);
            CVector rayEnd(x, y, z - 50.0f);

            if (CWorld::ProcessVerticalLine(rayStart, rayEnd.z, colPoint, colEntity,
                true, false, false, false, true, false, nullptr)) {
                groundZ = colPoint.m_vecPoint.z;
                foundGround = true;
            }
        }

        return foundGround;
    }

    bool FindGroundZWithElevationLimit(float x, float y, float z, float& groundZ, float maxElevationDiff) {
        bool foundGround = false;
        groundZ = CWorld::FindGroundZFor3DCoord(x, y, z + 50.0f, &foundGround);

        if (!foundGround) {
            CColPoint colPoint;
            CEntity* colEntity = nullptr;

            CVector rayStart(x, y, z + 50.0f);
            CVector rayEnd(x, y, z - 50.0f);

            if (CWorld::ProcessVerticalLine(rayStart, rayEnd.z, colPoint, colEntity,
                true, false, false, false, true, false, nullptr)) {
                groundZ = colPoint.m_vecPoint.z;
                foundGround = true;
            }
        }

        // Check elevation difference
        if (foundGround && std::abs(groundZ - z) > maxElevationDiff) {
            return false;  // Ground is too high/low
        }

        return foundGround;
    }

    bool IsVisibleFromPlayer(const CVector& spawnPos, const CVector& playerPos) {
        if (Dist3D(spawnPos, playerPos) < 15.0f) return true;
        if (Dist2D(spawnPos, playerPos) < 25.0f) return true;

        CColPoint colPoint;
        CEntity* colEntity = nullptr;

        return !CWorld::ProcessLineOfSight(playerPos, spawnPos, colPoint, colEntity,
            true, true, true, true, true, true, true);
    }

    // Enemy configuration
    void ConfigureEnemyPed(CPed* ped, ePedType gangType, int waveIndex, CPlayerPed* targetPlayer) {
        if (!ped || !targetPlayer) return;

        ped->m_nCharCreatedBy = MISSION_CHAR;
        ped->m_nAttackTimer = 0;
        ped->bRespondsToThreats = true;

        // CLEAR ALL EXISTING WEAPONS FIRST
        ped->ClearWeapons();  // This removes ALL weapons the ped might have

        // Give ONE random weapon from allowed list
        WaveConfig::WeaponOption weapon = WaveConfig::ChooseRandomWeapon(waveIndex);

        // Set ammo to a reasonable amount (not too much)
        unsigned int adjustedAmmo = weapon.ammo;
        if (weapon.weapon == WEAPONTYPE_BASEBALLBAT) {
            adjustedAmmo = 1;  // Melee weapons don't need ammo
        }
        else if (weapon.weapon == WEAPONTYPE_COLT45) {
            adjustedAmmo = std::min(weapon.ammo, 36u);  // Cap at 36 for pistol
        }
        else if (weapon.weapon == WEAPONTYPE_UZI) {
            adjustedAmmo = std::min(weapon.ammo, 120u); // Cap at 120 for Uzi
        }
        else if (weapon.weapon == WEAPONTYPE_AK47) {
            adjustedAmmo = std::min(weapon.ammo, 90u); // Cap at 90 for AK
        }

        // Give the weapon
        ped->GiveWeapon(weapon.weapon, adjustedAmmo);
        ped->SetCurrentWeapon(weapon.weapon);

        // Set objective
        ped->SetObjective(OBJECTIVE_KILL_CHAR_ON_FOOT, targetPlayer);

        // Set movement state
        if (waveIndex >= 1) {
            if (Rand01() < 0.4f) {
                ped->SetMoveState(PEDMOVE_RUN);
            }
            else if (Rand01() < 0.3f) {
                ped->SetMoveState(PEDMOVE_SPRINT);
            }
        }

        DebugLog::Write("Configured ped with weapon %d (ammo: %d)",
            (int)weapon.weapon, adjustedAmmo);
    }
}