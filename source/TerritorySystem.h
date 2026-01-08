#pragma once

#include "plugin.h"
#include "CVector.h"

#include <vector>
#include <string>
#include <cmath>

struct Territory {
    std::string id;
    float minX, minY, maxX, maxY;  // Axis-aligned rectangle
    int ownerGang;                 // ePedType: -1 neutral, PEDTYPE_GANG1 = Diablos, etc.
    bool underAttack;
    int defenseLevel;              // 0=Light, 1=Moderate, 2=Heavy

    Territory()
        : minX(0), minY(0), maxX(0), maxY(0), ownerGang(-1), underAttack(false), defenseLevel(1) {}

    bool ContainsPoint(const CVector& pos) const {
        return pos.x >= minX && pos.x <= maxX && pos.y >= minY && pos.y <= maxY;
    }

    float GetRadius() const {
        float halfW = (maxX - minX) * 0.5f;
        float halfH = (maxY - minY) * 0.5f;
        return std::sqrt(halfW * halfW + halfH * halfH);
    }
};

class TerritorySystem {
public:
    static void Init();
    static void Shutdown();

    // Game tick: hot reload poll
    static void Update();
    static inline void Process() { Update(); }

    // Query
    static const Territory* GetTerritoryAtPoint(const CVector& pos);
    static const Territory* GetTerritoryAtPlayer();
    static bool HasRealTerritories();

    // Ownership / state (persists to territories.txt)
    static void SetTerritoryOwner(const Territory* t, int newOwnerGang);
    static int GetPlayerGang();  // placeholder behavior preserved
    static void SetUnderAttack(const Territory* t, bool underAttack);

    // Territory radar overlay
    static void ToggleOverlay();
    static bool IsOverlayEnabled();
    static const std::vector<Territory>& GetTerritories();
    static void DrawRadarOverlay();

    // Config
    static void ForceReloadNow();

    // Editor API (Main.cpp owns keybinds)
    static void EditorToggle();
    static bool EditorEnabled();

    static void EditorSetCornerAAtPlayer();
    static void EditorSetCornerBAtPlayer();
    static void EditorCommitTerritory();          // adds + saves
    static void EditorDeleteClosestToPlayer();    // deletes + saves

private:
    static std::vector<Territory> s_territories;

    // Radar territory overlay
    static bool s_overlayEnabled;

    // Hot reload
    static unsigned int s_nextReloadPollMs;
    static long long s_lastConfigStamp;
    static unsigned int s_lastReloadFailToastMs;

    // Editor state
    struct EditorState {
        bool enabled = false;
        bool hasA = false;
        bool hasB = false;
        float ax = 0.0f, ay = 0.0f;
        float bx = 0.0f, by = 0.0f;
        int nextId = 1000;
        int defaultOwnerGang = -1;
        int defaultDefenseLevel = 1;
    };
    static EditorState s_editor;

private:
    static const char* ConfigPath();

    static void NormalizeRect(Territory& t);

    static long long GetConfigStampOrNeg1();

    static bool LoadFromFile(std::vector<Territory>& out, std::string& outErr);
    static bool SaveToFile(const std::vector<Territory>& terrs, std::string& outErr);

    static void HotReloadTick(unsigned int nowMs);
    static void TryReloadNow(bool showToastOnFail);

    // Editor helpers
    static bool GetPlayerXY(float& outX, float& outY);
    static int ComputeNextId(const std::vector<Territory>& terrs);
};
