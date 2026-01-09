// WaveManager.h (REFACTORED)
#pragma once
#include "TerritorySystem.h"
#include "plugin.h"
#include "ePedType.h"
#include <vector>

class CPickup;

class WaveManager {
public:
    enum class WarState {
        Idle,
        BetweenWaves,
        Spawning,           // NEW: Staggered spawning state
        Combat,
        VictoryDelay,
        Completed
    };

    // Core API
    static void Initialize();
    static void Shutdown();

    static void StartWar(ePedType defendingGang, const Territory* territory = nullptr);
    static void CancelWar();

    static void Update();
    static inline void Process() { Update(); }

    // Status queries
    static bool IsWarActive();
    static int GetCurrentWaveIndex();
    static int GetWaveTargetCount();
    static int GetWaveSpawnedSoFar();
    static int GetAliveCount();
    static const Territory* GetActiveTerritory();
    static ePedType GetDefendingGang();
    static WarState GetCurrentState() { return s_state; }

    // Pickup system
    static void SpawnInitialHealthPickup();         // Single health pickup at war start
    static void SpawnWaveArmorPickup();             // Single armor after wave 1 and wave 2
    static void CleanupWarPickups();                // Called 60s after war ends
    static void UpdatePickupCleanup();              // Check cleanup timer

    static bool ArePickupsActive() { return s_pickupsActive; }

private:
    // Wave progression
    static void BeginWave(int waveIndex);
    static void SpawnNextCluster();  // NEW: Spawn next cluster in wave
    static void CheckWaveCompletion();
    static void CompleteWar();

    // Timing constants
    static constexpr int s_waveDelayMs = 10000;    // 10 second delay in between waves
    static constexpr int s_initialDelayMs = 4000;  // 4 second initial delay
    static constexpr int s_victoryDelayMs = 2000;  // 2 second victory delay
    static constexpr int s_clusterDelayMs = 1000;  // 1 second between clusters
    static constexpr int s_maxWaves = 3;           // 3 waves in total

    // Pickup system tracking
    static int s_healthPickupHandle;
    static int s_armorPickupHandle;
    static bool s_pickupsActive;
    static unsigned int s_pickupCleanupTime;

    //Pickup helper functions
    static int SpawnPickupAtPosition_Handle(const CVector& pos, int pickupType, int modelId, int quantity);
    static CVector FindPickupPositionInTerritory(const Territory* territory, CPickup* avoidPickup = nullptr);
    static CPickup* ResolvePickup(int handle);
    static void CleanupPickup(int& handle);

    // Flee system
    static CVector s_warCenter;
    static float s_warRadius;
    static constexpr float s_fleeRadiusMultiplier = 1.5f;
    static constexpr unsigned int s_fleeCheckIntervalMs = 500; // Check player fleeing every 500ms

    // Flee methods
    static void CheckForFleeing();
    static void CheckPlayerDeath();
    static float Dist2D(const CVector& a, const CVector& b);

    // Delayed wave completion messages
    static const unsigned int s_waveCompletionMessageDelayMs = 800; // 800ms delay
    static unsigned int s_showWaveMessageAtTime;
    static int s_pendingWaveMessage;
    static void ScheduleWaveCompletionMessage(int completedWave, unsigned int now);
    static void ShowWaveCompletionMessage(int waveIndex);

    // Wanted Level suppression
    static void FreezeWantedLevelDuringWar();
    static bool IsWantedLevelSuppressionNeeded();
    static int s_originalWantedLevel;
    static unsigned char s_originalWantedFlags;
    static int s_originalChaosLevel;
    static bool s_wantedLevelFrozen;

    // State
    static WarState s_state;
    static int s_currentWave;
    static int s_enemiesSpawned;
    static int s_enemiesTarget;
    static unsigned int s_nextActionTime;
    static unsigned int s_nextClusterSpawnTime;  // NEW: Time for next cluster
    static ePedType s_defendingGang;
    static const Territory* s_activeTerritory;
    static bool s_isShuttingDown;

    // NEW: Cluster spawning state
    static std::vector<CVector> s_clusterCenters;
    static std::vector<int> s_clusterSizes;
    static size_t s_currentClusterIndex;
    static int s_enemiesSpawnedInWave;
};