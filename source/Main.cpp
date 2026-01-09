#include "plugin.h"
#include "GangInfo.h"
#include "CMessages.h"
#include "DebugLog.h"
#include "WaveManager.h"
#include "TerritorySystem.h"
#include "WarSystem.h"
#include "CRadar.h"
#include "CWorld.h"
#include "CModelInfo.h"
#include "CBaseModelInfo.h"
#include "PedDeathTracker.h"
#include "DamageHook.h"
#include "DirectDamageTracker.h"
#include "TerritoryPersistence.h"

#include <windows.h>
#include <cstdio>

static bool g_isTearingDown = false;
using namespace plugin;

static bool JustPressed(int vk)
{
    return (GetAsyncKeyState(vk) & 1) != 0;
}

static int ResolveModelId(const char* modelName)
{
    if (!modelName || !modelName[0]) {
        CMessages::AddMessageJumpQ("Model: No name", 1500, 0);
        return -1;
    }

    CMessages::AddMessageJumpQ("Model: Looking up...", 1500, 0);

    int index = -1;
    CBaseModelInfo* mi = CModelInfo::GetModelInfo(modelName, &index);

    if (index >= 0) {
        CMessages::AddMessageJumpQ("Model: Found", 1500, 0);
    }
    else {
        CMessages::AddMessageJumpQ("Model: NOT found", 1500, 0);
    }

    return index;
}

class GangTerritoryWarsMain
{
public:
    GangTerritoryWarsMain()
    {
        Events::initRwEvent += [] {
            DebugLog::Initialize("III.GangTerritoryWars.log");
            GangManager::Initialize();
            WaveManager::Initialize();
            TerritorySystem::Init();
            TerritoryPersistence::Init();
            WarSystem::Init();

            DirectDamageTracker::Initialize();
            PedDeathTracker::Initialize();

            DamageHook::Install();

            DebugLog::Write("GangTerritoryWars loaded");
            };

        Events::d3dLostEvent += [] {
            DebugLog::Write("Direct3D device lost");
            };

        Events::d3dResetEvent += [] {
            DebugLog::Write("Direct3D device reset");
            };

        Events::shutdownRwEvent += [] {
            DebugLog::Write("Plugin shutdown triggered via shutdownRwEvent");
            g_isTearingDown = true;

            TerritoryPersistence::Shutdown();
            TerritorySystem::Shutdown();
            PedDeathTracker::Shutdown();
            DirectDamageTracker::Shutdown();
            WaveManager::Shutdown();
            DebugLog::Shutdown();
            };

        Events::gameProcessEvent += [] {
            if (g_isTearingDown) return;

            TerritorySystem::Process();
            TerritoryPersistence::Process();
            WarSystem::Process();
            WaveManager::Process();

            DirectDamageTracker::Process();
            PedDeathTracker::Process();

            // Existing hotkeys you already had
            if (JustPressed(VK_F8)) {
                WaveManager::CancelWar();
                CMessages::AddMessageJumpQ("Gang war cancelled", 1400, 0);
            }
            if (JustPressed(VK_F6)) {
                TerritorySystem::ToggleOverlay();
            }
            if (JustPressed(VK_F9)) {
                CPlayerPed* player = CWorld::Players[0].m_pPed;
                if (player) {
                    const CVector pos = player->GetPosition();
                    char msg[64];
                    sprintf(msg, "Player Pos: %.1f %.1f %.1f", pos.x, pos.y, pos.z);
                    CMessages::AddMessageJumpQ(msg, 5000, 0);
                }
            }

            // Territory editor (NUMPAD)
            if (JustPressed(VK_NUMPAD0)) TerritorySystem::EditorToggle();
            if (JustPressed(VK_NUMPAD7)) TerritorySystem::EditorSetCornerAAtPlayer();
            if (JustPressed(VK_NUMPAD9)) TerritorySystem::EditorSetCornerBAtPlayer();
            if (JustPressed(VK_NUMPAD5)) TerritorySystem::EditorCommitTerritory();
            if (JustPressed(VK_NUMPAD8)) TerritorySystem::EditorDeleteClosestToPlayer();
            if (JustPressed(VK_NUMPAD1)) TerritorySystem::ForceReloadNow();
            if (JustPressed(VK_NUMPAD2)) TerritorySystem::ToggleOverlay();
            };

        Events::drawRadarMapEvent += []() {
            if (g_isTearingDown) return;
            TerritorySystem::DrawRadarOverlay();
            };
    }
} gangTerritoryWarsMain;
