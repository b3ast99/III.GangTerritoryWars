#pragma once
#include <vector>

struct Territory;

// All radar drawing + RenderWare Im2D helpers live here.
// TerritorySystem owns territory state/ownership; renderer is stateless.
class TerritoryRadarRenderer {
public:
    static void DrawRadarOverlay(const std::vector<Territory>& territories);
    static void ResetTransientState();
};
