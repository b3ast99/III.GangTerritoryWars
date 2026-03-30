#include "TestFramework.h"
#include "../source/TerritorySystem.h"

// Helpers
static Territory MakeTerritory(float minX, float minY, float maxX, float maxY) {
    Territory t;
    t.minX = minX; t.minY = minY;
    t.maxX = maxX; t.maxY = maxY;
    return t;
}

void RunTerritoryAabbTests(Test::Runner& t) {
    t.suite("Territory AABB");

    // ------------------------------------------------------------------
    // ContainsPoint — basic cases
    // ------------------------------------------------------------------
    t.run("contains: point clearly inside", [&] {
        auto terr = MakeTerritory(0.f, 0.f, 100.f, 100.f);
        REQUIRE(terr.ContainsPoint(CVector(50.f, 50.f, 0.f)));
    });

    t.run("contains: point clearly outside (right)", [&] {
        auto terr = MakeTerritory(0.f, 0.f, 100.f, 100.f);
        REQUIRE_FALSE(terr.ContainsPoint(CVector(150.f, 50.f, 0.f)));
    });

    t.run("contains: point clearly outside (left)", [&] {
        auto terr = MakeTerritory(0.f, 0.f, 100.f, 100.f);
        REQUIRE_FALSE(terr.ContainsPoint(CVector(-1.f, 50.f, 0.f)));
    });

    t.run("contains: point clearly outside (above)", [&] {
        auto terr = MakeTerritory(0.f, 0.f, 100.f, 100.f);
        REQUIRE_FALSE(terr.ContainsPoint(CVector(50.f, 101.f, 0.f)));
    });

    t.run("contains: point clearly outside (below)", [&] {
        auto terr = MakeTerritory(0.f, 0.f, 100.f, 100.f);
        REQUIRE_FALSE(terr.ContainsPoint(CVector(50.f, -1.f, 0.f)));
    });

    // ------------------------------------------------------------------
    // ContainsPoint — boundary (inclusive)
    // ------------------------------------------------------------------
    t.run("contains: point on minX boundary is inside", [&] {
        auto terr = MakeTerritory(10.f, 10.f, 90.f, 90.f);
        REQUIRE(terr.ContainsPoint(CVector(10.f, 50.f, 0.f)));
    });

    t.run("contains: point on maxX boundary is inside", [&] {
        auto terr = MakeTerritory(10.f, 10.f, 90.f, 90.f);
        REQUIRE(terr.ContainsPoint(CVector(90.f, 50.f, 0.f)));
    });

    t.run("contains: point on minY boundary is inside", [&] {
        auto terr = MakeTerritory(10.f, 10.f, 90.f, 90.f);
        REQUIRE(terr.ContainsPoint(CVector(50.f, 10.f, 0.f)));
    });

    t.run("contains: point on maxY boundary is inside", [&] {
        auto terr = MakeTerritory(10.f, 10.f, 90.f, 90.f);
        REQUIRE(terr.ContainsPoint(CVector(50.f, 90.f, 0.f)));
    });

    t.run("contains: corner point is inside", [&] {
        auto terr = MakeTerritory(0.f, 0.f, 50.f, 50.f);
        REQUIRE(terr.ContainsPoint(CVector(0.f, 0.f, 0.f)));
        REQUIRE(terr.ContainsPoint(CVector(50.f, 50.f, 0.f)));
    });

    // ------------------------------------------------------------------
    // ContainsPoint — Z is ignored (territories are 2D)
    // ------------------------------------------------------------------
    t.run("contains: z coordinate is ignored", [&] {
        auto terr = MakeTerritory(0.f, 0.f, 100.f, 100.f);
        REQUIRE(terr.ContainsPoint(CVector(50.f, 50.f, 999.f)));
        REQUIRE(terr.ContainsPoint(CVector(50.f, 50.f, -999.f)));
    });

    // ------------------------------------------------------------------
    // ContainsPoint — GTA III coordinate ranges (Portland-ish values)
    // ------------------------------------------------------------------
    t.run("contains: realistic Portland coordinates", [&] {
        // Portland roughly spans -1000..600 x, -200..1200 y in GTA III
        auto terr = MakeTerritory(-800.f, 400.f, -400.f, 800.f);
        REQUIRE(terr.ContainsPoint(CVector(-600.f, 600.f, 10.f)));
        REQUIRE_FALSE(terr.ContainsPoint(CVector(-900.f, 600.f, 10.f)));
    });

    t.run("contains: negative coordinate territory", [&] {
        auto terr = MakeTerritory(-500.f, -500.f, -100.f, -100.f);
        REQUIRE(terr.ContainsPoint(CVector(-300.f, -300.f, 0.f)));
        REQUIRE_FALSE(terr.ContainsPoint(CVector(0.f, 0.f, 0.f)));
    });

    // ------------------------------------------------------------------
    // GetRadius — sanity checks
    // ------------------------------------------------------------------
    t.run("radius: square territory has expected radius", [&] {
        // 100x100 square: half-diag = sqrt(50^2 + 50^2) = ~70.7
        auto terr = MakeTerritory(0.f, 0.f, 100.f, 100.f);
        const float r = terr.GetRadius();
        REQUIRE(r > 70.f && r < 72.f);
    });

    t.run("radius: zero-size territory has zero radius", [&] {
        auto terr = MakeTerritory(50.f, 50.f, 50.f, 50.f);
        REQUIRE(terr.GetRadius() == 0.f);
    });
}
