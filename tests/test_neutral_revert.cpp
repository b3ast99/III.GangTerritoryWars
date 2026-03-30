#include "TestFramework.h"
#include "../source/NeutralRevertRule.h"

static constexpr int MAFIA   = 7;
static constexpr int TRIADS  = 8;
static constexpr int DIABLOS = 9;
static constexpr int NEUTRAL = -1;

static constexpr unsigned int REVERT_MS = 180000; // 3 minutes

void RunNeutralRevertTests(Test::Runner& t) {
    t.suite("NeutralRevertRule");

    // ------------------------------------------------------------------
    // NeutralRevertDue
    // ------------------------------------------------------------------
    t.run("not due: no time elapsed", [&] {
        REQUIRE_FALSE(NeutralRevertDue(1000, 1000, REVERT_MS));
    });

    t.run("not due: just under the window", [&] {
        REQUIRE_FALSE(NeutralRevertDue(0, REVERT_MS - 1, REVERT_MS));
    });

    t.run("due: exactly at window boundary", [&] {
        // neutralSinceMs must be non-zero (0 = not stamped)
        REQUIRE(NeutralRevertDue(1, REVERT_MS + 1, REVERT_MS));
    });

    t.run("due: well past the window", [&] {
        REQUIRE(NeutralRevertDue(1, REVERT_MS + 60000, REVERT_MS));
    });

    t.run("not due: revertMs = 0 disables revert", [&] {
        // Even if plenty of time has passed, 0 means disabled
        REQUIRE_FALSE(NeutralRevertDue(0, 999999999, 0));
    });

    t.run("not due: neutralSinceMs = 0 means not stamped", [&] {
        // Territory never went neutral (or was cleared) — should not revert
        REQUIRE_FALSE(NeutralRevertDue(0, REVERT_MS + 1, REVERT_MS));
    });

    t.run("due: territory went neutral partway through session", [&] {
        const unsigned int wentNeutralAt = 50000;
        const unsigned int now = wentNeutralAt + REVERT_MS;
        REQUIRE(NeutralRevertDue(wentNeutralAt, now, REVERT_MS));
    });

    t.run("not due: territory went neutral recently", [&] {
        const unsigned int wentNeutralAt = 50000;
        const unsigned int now = wentNeutralAt + REVERT_MS - 1;
        REQUIRE_FALSE(NeutralRevertDue(wentNeutralAt, now, REVERT_MS));
    });

    // ------------------------------------------------------------------
    // NeutralRevertTarget
    // ------------------------------------------------------------------
    t.run("target: lastOwnerGang preferred over defaultOwnerGang", [&] {
        REQUIRE_EQ(NeutralRevertTarget(TRIADS, MAFIA), TRIADS);
    });

    t.run("target: falls back to defaultOwnerGang when lastOwner unknown", [&] {
        REQUIRE_EQ(NeutralRevertTarget(NEUTRAL, MAFIA), MAFIA);
    });

    t.run("target: returns -1 when both unknown", [&] {
        REQUIRE_EQ(NeutralRevertTarget(NEUTRAL, NEUTRAL), NEUTRAL);
    });

    t.run("target: lastOwner = DIABLOS, default = TRIADS -> DIABLOS", [&] {
        REQUIRE_EQ(NeutralRevertTarget(DIABLOS, TRIADS), DIABLOS);
    });

    // ------------------------------------------------------------------
    // Combined: simulate the full revert check as done in Update()
    // ------------------------------------------------------------------
    t.run("combined: territory due to revert with known last owner", [&] {
        const unsigned int neutralSince = 1000; // non-zero: territory was stamped at t=1000ms
        const unsigned int now = neutralSince + REVERT_MS;
        const bool due = NeutralRevertDue(neutralSince, now, REVERT_MS);
        const int target = NeutralRevertTarget(TRIADS, MAFIA);
        REQUIRE(due);
        REQUIRE_EQ(target, TRIADS);
    });

    t.run("combined: territory not due even with known last owner", [&] {
        const unsigned int neutralSince = 1000;
        const unsigned int now = neutralSince + REVERT_MS - 500;
        REQUIRE_FALSE(NeutralRevertDue(neutralSince, now, REVERT_MS));
    });

    t.run("combined: disabled revert never triggers regardless of time", [&] {
        REQUIRE_FALSE(NeutralRevertDue(0, 999999999, 0));
        // target would be valid but revert is blocked upstream
    });
}
