// WaveSpawning.h
#pragma once
#include "CVector.h"
#include "ePedType.h"
#include <vector>

class CPed;
class CPlayerPed;
struct Territory;

namespace WaveSpawning {
    struct SpawnResult {
        CPed* ped = nullptr;
        int pedHandle = -1;
        CVector position;
    };

    // Core spawning
    std::vector<SpawnResult> SpawnWaveEnemies(
        ePedType gangType,
        const Territory* territory,
        int waveIndex,
        int targetCount
    );

    // Spawning helpers
    int CalculateClusterCount(int targetCount);
    std::vector<int> CalculateClusterSizes(int targetCount, int numClusters);
    std::vector<CVector> FindClusterCenters(
        const Territory* territory,
        const CVector& playerPos,
        int numClusters,
        int waveIndex);
    CVector CreateFallbackClusterCenter(const CVector& playerPos, const Territory* territory);
    CVector FindAdditionalClusterCenter(
        const Territory* territory,
        const CVector& playerPos,
        int waveIndex,
        const std::vector<CVector>& existingCenters,
        int clusterIndex);
    CVector CreateForcedClusterCenter(
        const CVector& firstCenter,
        const std::vector<CVector>& existingCenters,
        const Territory* territory,
        int clusterIndex);
    std::vector<SpawnResult> SpawnClusterEnemies(
        ePedType gangType,
        const Territory* territory,
        int waveIndex,
        CPlayerPed* player,
        const CVector& clusterCenter,
        int enemiesToSpawn,
        int& totalSpawnedCounter);
    int GetEnemyModelId(ePedType gangType);
    CVector CalculateSpawnPosition(const CVector& clusterCenter, const Territory* territory);
    CPed* SpawnSingleEnemy(ePedType gangType, int modelId, const CVector& position);
    SpawnResult CreateSpawnResult(CPed* ped, const CVector& position);

    // Clusters
    struct WaveSpawnPlan {
        std::vector<CVector> clusterCenters;
        std::vector<int> clusterSizes;
    };

    // And add the function declaration:
    WaveSpawnPlan PlanWaveSpawn(
        ePedType gangType,
        const Territory* territory,
        int waveIndex,
        int targetCount
    );

    // Add the SpawnSingleClusterEnemies function declaration:
    std::vector<SpawnResult> SpawnSingleClusterEnemies(
        ePedType gangType,
        const Territory* territory,
        int waveIndex,
        const CVector& clusterCenter,
        int enemiesInCluster
    );

    // Position finding
    bool FindStrategicSpawnPosition(
        CVector& outPos,
        const Territory* territory,
        const CVector& playerPos,
        const std::vector<CVector>& existingSpawns,
        int waveIndex
    );

    // Environment checks
    bool IsPositionInWater(const CVector& pos);
    bool IsPositionOnRoof(const CVector& pos);
    bool FindGroundZForCoord(float x, float y, float z, float& groundZ);
    bool FindGroundZWithElevationLimit(float x, float y, float z, float& groundZ, float maxElevationDiff);
    bool IsVisibleFromPlayer(const CVector& spawnPos, const CVector& playerPos);

    // Enemy configuration
    void ConfigureEnemyPed(CPed* ped, ePedType gangType, int waveIndex, CPlayerPed* targetPlayer);
}