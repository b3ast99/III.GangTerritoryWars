#include "Geometry.h"

#include <cmath>

namespace Geometry
{
    bool IsPointInCircle2D(const CVector& point, const CVector& center, float radius)
    {
        const float dx = point.x - center.x;
        const float dy = point.y - center.y;
        return (dx * dx + dy * dy) <= (radius * radius);
    }
}
