#include "TestFramework.h"
#include "../source/SaveSlotParser.h"

void RunSaveSlotParserTests(Test::Runner& t) {
    t.suite("SaveSlotParser");

    // ------------------------------------------------------------------
    // TryParseSaveSlot — valid paths
    // ------------------------------------------------------------------
    t.run("slot: parses slot 1 from standard path", [&] {
        int slot = 0;
        REQUIRE(SaveSlotParser::TryParseSaveSlot("GTA3sf1.b", slot));
        REQUIRE_EQ(slot, 1);
    });

    t.run("slot: parses slot 8 from standard path", [&] {
        int slot = 0;
        REQUIRE(SaveSlotParser::TryParseSaveSlot("GTA3sf8.b", slot));
        REQUIRE_EQ(slot, 8);
    });

    t.run("slot: parses from full Windows path", [&] {
        int slot = 0;
        REQUIRE(SaveSlotParser::TryParseSaveSlot(
            "C:\\Users\\Player\\AppData\\Local\\GTA3 User Files\\GTA3sf3.b", slot));
        REQUIRE_EQ(slot, 3);
    });

    t.run("slot: parses from path with forward slashes", [&] {
        int slot = 0;
        REQUIRE(SaveSlotParser::TryParseSaveSlot("saves/GTA3sf5.b", slot));
        REQUIRE_EQ(slot, 5);
    });

    // ------------------------------------------------------------------
    // TryParseSaveSlot — invalid paths
    // ------------------------------------------------------------------
    t.run("slot: rejects null path", [&] {
        int slot = 0;
        REQUIRE_FALSE(SaveSlotParser::TryParseSaveSlot(nullptr, slot));
    });

    t.run("slot: rejects slot 0 (out of range)", [&] {
        int slot = 0;
        REQUIRE_FALSE(SaveSlotParser::TryParseSaveSlot("GTA3sf0.b", slot));
    });

    t.run("slot: rejects slot 9 (out of range)", [&] {
        int slot = 0;
        REQUIRE_FALSE(SaveSlotParser::TryParseSaveSlot("GTA3sf9.b", slot));
    });

    t.run("slot: rejects wrong extension (.bak)", [&] {
        int slot = 0;
        REQUIRE_FALSE(SaveSlotParser::TryParseSaveSlot("GTA3sf1.bak", slot));
    });

    t.run("slot: rejects wrong extension (.sav)", [&] {
        int slot = 0;
        REQUIRE_FALSE(SaveSlotParser::TryParseSaveSlot("GTA3sf1.sav", slot));
    });

    t.run("slot: rejects wrong prefix", [&] {
        int slot = 0;
        REQUIRE_FALSE(SaveSlotParser::TryParseSaveSlot("GTASAsf1.b", slot));
    });

    t.run("slot: rejects non-numeric after prefix", [&] {
        int slot = 0;
        REQUIRE_FALSE(SaveSlotParser::TryParseSaveSlot("GTA3sfA.b", slot));
    });

    t.run("slot: rejects empty string", [&] {
        int slot = 0;
        REQUIRE_FALSE(SaveSlotParser::TryParseSaveSlot("", slot));
    });

    // ------------------------------------------------------------------
    // IsReadMode
    // ------------------------------------------------------------------
    t.run("mode: \"r\" is read", [&] {
        REQUIRE(SaveSlotParser::IsReadMode("r"));
    });

    t.run("mode: \"rb\" is read", [&] {
        REQUIRE(SaveSlotParser::IsReadMode("rb"));
    });

    t.run("mode: \"w\" is not read", [&] {
        REQUIRE_FALSE(SaveSlotParser::IsReadMode("w"));
    });

    t.run("mode: \"wb\" is not read", [&] {
        REQUIRE_FALSE(SaveSlotParser::IsReadMode("wb"));
    });

    t.run("mode: \"a\" is not read", [&] {
        REQUIRE_FALSE(SaveSlotParser::IsReadMode("a"));
    });

    t.run("mode: null is not read", [&] {
        REQUIRE_FALSE(SaveSlotParser::IsReadMode(nullptr));
    });

    // ------------------------------------------------------------------
    // IsWriteMode
    // ------------------------------------------------------------------
    t.run("mode: \"w\" is write", [&] {
        REQUIRE(SaveSlotParser::IsWriteMode("w"));
    });

    t.run("mode: \"wb\" is write", [&] {
        REQUIRE(SaveSlotParser::IsWriteMode("wb"));
    });

    t.run("mode: \"a\" is write (append)", [&] {
        REQUIRE(SaveSlotParser::IsWriteMode("a"));
    });

    t.run("mode: \"r\" is not write", [&] {
        REQUIRE_FALSE(SaveSlotParser::IsWriteMode("r"));
    });

    t.run("mode: \"rb\" is not write", [&] {
        REQUIRE_FALSE(SaveSlotParser::IsWriteMode("rb"));
    });

    t.run("mode: null is not write", [&] {
        REQUIRE_FALSE(SaveSlotParser::IsWriteMode(nullptr));
    });
}
