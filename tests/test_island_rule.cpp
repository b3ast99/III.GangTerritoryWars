#include "TestFramework.h"
#include "../source/IslandRule.h"

void RunIslandRuleTests(Test::Runner& t) {
    t.suite("IslandRule – GetIslandForPosition");

    // Portland: x > 616
    t.run("deep Portland (x=900) -> PORTLAND", [&] {
        REQUIRE(GetIslandForPosition(900.0f, 0.0f) == Island::PORTLAND);
    });

    t.run("Portland boundary (x=617) -> PORTLAND", [&] {
        REQUIRE(GetIslandForPosition(617.0f, 100.0f) == Island::PORTLAND);
    });

    t.run("exact boundary (x=616.01) -> PORTLAND", [&] {
        REQUIRE(GetIslandForPosition(616.01f, 0.0f) == Island::PORTLAND);
    });

    // Staunton: -378 < x <= 616
    t.run("Staunton center (x=100) -> STAUNTON", [&] {
        REQUIRE(GetIslandForPosition(100.0f, 200.0f) == Island::STAUNTON);
    });

    t.run("Staunton boundary (x=616) -> STAUNTON", [&] {
        REQUIRE(GetIslandForPosition(616.0f, 0.0f) == Island::STAUNTON);
    });

    t.run("Staunton near west boundary (x=-377) -> STAUNTON", [&] {
        REQUIRE(GetIslandForPosition(-377.0f, 0.0f) == Island::STAUNTON);
    });

    t.run("Staunton origin (x=0) -> STAUNTON", [&] {
        REQUIRE(GetIslandForPosition(0.0f, 0.0f) == Island::STAUNTON);
    });

    // Shoreside: x <= -378
    t.run("Shoreside center (x=-500) -> SHORESIDE", [&] {
        REQUIRE(GetIslandForPosition(-500.0f, 0.0f) == Island::SHORESIDE);
    });

    t.run("Shoreside boundary (x=-378) -> SHORESIDE", [&] {
        REQUIRE(GetIslandForPosition(-378.0f, 0.0f) == Island::SHORESIDE);
    });

    t.run("deep Shoreside (x=-1500) -> SHORESIDE", [&] {
        REQUIRE(GetIslandForPosition(-1500.0f, 300.0f) == Island::SHORESIDE);
    });

    // Y coordinate should not affect island
    t.run("y coordinate has no effect", [&] {
        REQUIRE(GetIslandForPosition(900.0f, -9999.0f) == Island::PORTLAND);
        REQUIRE(GetIslandForPosition(900.0f, 9999.0f) == Island::PORTLAND);
        REQUIRE(GetIslandForPosition(0.0f, -9999.0f) == Island::STAUNTON);
    });

    // ------------------------------------------------------------------
    // IsIslandUnlocked
    // ------------------------------------------------------------------
    t.suite("IslandRule – IsIslandUnlocked");

    t.run("Act 0 (NONE): nothing unlocked", [&] {
        REQUIRE_FALSE(IsIslandUnlocked(Island::PORTLAND, 0));
        REQUIRE_FALSE(IsIslandUnlocked(Island::STAUNTON, 0));
        REQUIRE_FALSE(IsIslandUnlocked(Island::SHORESIDE, 0));
    });

    t.run("Act 1: only Portland unlocked", [&] {
        REQUIRE(IsIslandUnlocked(Island::PORTLAND, 1));
        REQUIRE_FALSE(IsIslandUnlocked(Island::STAUNTON, 1));
        REQUIRE_FALSE(IsIslandUnlocked(Island::SHORESIDE, 1));
    });

    t.run("Act 2: Portland + Staunton unlocked", [&] {
        REQUIRE(IsIslandUnlocked(Island::PORTLAND, 2));
        REQUIRE(IsIslandUnlocked(Island::STAUNTON, 2));
        REQUIRE_FALSE(IsIslandUnlocked(Island::SHORESIDE, 2));
    });

    t.run("Act 3: all islands unlocked", [&] {
        REQUIRE(IsIslandUnlocked(Island::PORTLAND, 3));
        REQUIRE(IsIslandUnlocked(Island::STAUNTON, 3));
        REQUIRE(IsIslandUnlocked(Island::SHORESIDE, 3));
    });

    t.run("UNKNOWN island is never unlocked", [&] {
        REQUIRE_FALSE(IsIslandUnlocked(Island::UNKNOWN, 0));
        REQUIRE_FALSE(IsIslandUnlocked(Island::UNKNOWN, 1));
        REQUIRE_FALSE(IsIslandUnlocked(Island::UNKNOWN, 3));
    });
}
