#pragma once
#include "plugin.h"

class TerritoryAmbientSpawner {
public:
    static void Init();
    static void Shutdown();

    static void Update();
    static inline void Process() { Update(); }

    // Optional runtime tuning (defaults are sane)
    static void SetEnabled(bool enabled);
    static bool IsEnabled();
};
