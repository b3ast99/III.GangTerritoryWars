#include "TestFramework.h"
#include "../source/ActTransitionRule.h"

void RunActTransitionRuleTests(Test::Runner& t) {

    // ==================================================================
    // Known trigger labels — each only fires from the correct act floor
    // ==================================================================
    t.suite("ActTransitionRule – JM2 (Farewell Chunky / Act 0->1)");

    t.run("JM2 advances act 0 to 1", [&] {
        REQUIRE_EQ(GetActForMissionLabel("JM2", 0), 1);
    });

    t.run("JM2 does nothing if already act 1", [&] {
        REQUIRE_EQ(GetActForMissionLabel("JM2", 1), 1);
    });

    t.run("JM2 does nothing if already act 2", [&] {
        REQUIRE_EQ(GetActForMissionLabel("JM2", 2), 2);
    });

    t.run("JM2 does nothing if already act 3", [&] {
        REQUIRE_EQ(GetActForMissionLabel("JM2", 3), 3);
    });

    t.suite("ActTransitionRule – FM4 (Last Requests / Act 1->2)");

    t.run("FM4 advances act 1 to 2", [&] {
        REQUIRE_EQ(GetActForMissionLabel("FM4", 1), 2);
    });

    t.run("FM4 does nothing at act 0 (JM2 not yet passed)", [&] {
        // Player somehow triggers FM4 before JM2 — act 0 is below the floor of 2
        REQUIRE_EQ(GetActForMissionLabel("FM4", 0), 0);
    });

    t.run("FM4 does nothing if already act 2", [&] {
        REQUIRE_EQ(GetActForMissionLabel("FM4", 2), 2);
    });

    t.run("FM4 does nothing if already act 3", [&] {
        REQUIRE_EQ(GetActForMissionLabel("FM4", 3), 3);
    });

    t.suite("ActTransitionRule – LOVE2 (Waka-Gashira / Act 2->3)");

    t.run("LOVE2 advances act 2 to 3", [&] {
        REQUIRE_EQ(GetActForMissionLabel("LOVE2", 2), 3);
    });

    t.run("LOVE2 does nothing at act 0", [&] {
        REQUIRE_EQ(GetActForMissionLabel("LOVE2", 0), 0);
    });

    t.run("LOVE2 does nothing at act 1", [&] {
        REQUIRE_EQ(GetActForMissionLabel("LOVE2", 1), 1);
    });

    t.run("LOVE2 does nothing if already act 3", [&] {
        REQUIRE_EQ(GetActForMissionLabel("LOVE2", 3), 3);
    });

    // ==================================================================
    // Non-trigger labels — should never change act
    // ==================================================================
    t.suite("ActTransitionRule – non-trigger labels");

    t.run("empty string does not change act", [&] {
        REQUIRE_EQ(GetActForMissionLabel("", 0), 0);
        REQUIRE_EQ(GetActForMissionLabel("", 1), 1);
    });

    t.run("null does not change act", [&] {
        REQUIRE_EQ(GetActForMissionLabel(nullptr, 0), 0);
        REQUIRE_EQ(GetActForMissionLabel(nullptr, 2), 2);
    });

    t.run("unrelated mission labels do not change act", [&] {
        REQUIRE_EQ(GetActForMissionLabel("JM1",  0), 0);  // Joey mission 1
        REQUIRE_EQ(GetActForMissionLabel("TM1",  1), 1);  // Toni mission 1
        REQUIRE_EQ(GetActForMissionLabel("LM1",  2), 2);  // Love mission 1
        REQUIRE_EQ(GetActForMissionLabel("KM1",  2), 2);  // Kenji mission 1
        REQUIRE_EQ(GetActForMissionLabel("HOOD1",2), 2);  // Hoods mission
    });

    t.run("partial label matches do not trigger (JM vs JM2)", [&] {
        REQUIRE_EQ(GetActForMissionLabel("JM",   0), 0);
        REQUIRE_EQ(GetActForMissionLabel("JM22", 0), 0);
        REQUIRE_EQ(GetActForMissionLabel("FM",   1), 1);
        REQUIRE_EQ(GetActForMissionLabel("LOVE", 2), 2);
        REQUIRE_EQ(GetActForMissionLabel("LOVE2X", 2), 2);
    });

    t.run("case sensitivity: lowercase labels do not trigger", [&] {
        REQUIRE_EQ(GetActForMissionLabel("jm2",   0), 0);
        REQUIRE_EQ(GetActForMissionLabel("fm4",   1), 1);
        REQUIRE_EQ(GetActForMissionLabel("love2", 2), 2);
    });

    // ==================================================================
    // Sequential transition chain
    // ==================================================================
    t.suite("ActTransitionRule – full act progression chain");

    t.run("full game progression: 0->1->2->3 in order", [&] {
        int act = 0;
        act = GetActForMissionLabel("JM2",   act); REQUIRE_EQ(act, 1);
        act = GetActForMissionLabel("FM4",   act); REQUIRE_EQ(act, 2);
        act = GetActForMissionLabel("LOVE2", act); REQUIRE_EQ(act, 3);
    });

    t.run("old labels re-seen after advancing do not regress act", [&] {
        int act = 3;
        act = GetActForMissionLabel("JM2",   act); REQUIRE_EQ(act, 3);
        act = GetActForMissionLabel("FM4",   act); REQUIRE_EQ(act, 3);
        act = GetActForMissionLabel("LOVE2", act); REQUIRE_EQ(act, 3);
    });
}
