#include "TestFramework.h"
#include "../source/SidecarFormat.h"

using namespace SidecarFormat;

// Helper: build a v1 legacy blob manually
static std::vector<unsigned char> BuildV1Blob(const std::vector<Entry>& entries) {
    const unsigned int kMagic  = 0x31575447u;
    const unsigned int kV1     = 1u;

    std::vector<unsigned char> out;
    auto pushU32 = [&](unsigned int v) {
        out.push_back(v & 0xFF); out.push_back((v>>8)&0xFF);
        out.push_back((v>>16)&0xFF); out.push_back((v>>24)&0xFF);
    };
    auto pushU16 = [&](unsigned short v) {
        out.push_back(v & 0xFF); out.push_back((v>>8)&0xFF);
    };

    pushU32(kMagic);
    pushU32(kV1);
    pushU32((unsigned int)entries.size());
    for (const auto& e : entries) {
        pushU16((unsigned short)e.id.size());
        out.insert(out.end(), e.id.begin(), e.id.end());
        pushU32((unsigned int)e.ownerGang);
    }
    return out;
}

void RunSidecarFormatTests(Test::Runner& t) {
    t.suite("SidecarFormat");

    // ------------------------------------------------------------------
    // Round-trip: v2 serialize -> deserialize
    // ------------------------------------------------------------------
    t.run("round-trip: empty entries", [&] {
        const auto bytes = Serialize({});
        const auto r = Deserialize(bytes);
        REQUIRE(r.ok);
        REQUIRE(r.entries.empty());
    });

    t.run("round-trip: single entry", [&] {
        std::vector<Entry> in = { {"1001", 7} };
        const auto r = Deserialize(Serialize(in));
        REQUIRE(r.ok);
        REQUIRE_EQ((int)r.entries.size(), 1);
        REQUIRE_EQ(r.entries[0].id, std::string("1001"));
        REQUIRE_EQ(r.entries[0].ownerGang, 7);
    });

    t.run("round-trip: multiple entries", [&] {
        std::vector<Entry> in = {
            {"1001", 7}, {"1002", 8}, {"1003", 9}, {"1004", -1}
        };
        const auto r = Deserialize(Serialize(in));
        REQUIRE(r.ok);
        REQUIRE_EQ((int)r.entries.size(), 4);
        REQUIRE_EQ(r.entries[0].ownerGang, 7);
        REQUIRE_EQ(r.entries[1].ownerGang, 8);
        REQUIRE_EQ(r.entries[2].ownerGang, 9);
        REQUIRE_EQ(r.entries[3].ownerGang, -1);
    });

    t.run("round-trip: player-owned territory (ownerGang = -1)", [&] {
        std::vector<Entry> in = { {"cleared", -1} };
        const auto r = Deserialize(Serialize(in));
        REQUIRE(r.ok);
        REQUIRE_EQ(r.entries[0].ownerGang, -1);
    });

    t.run("round-trip: neutral territory (ownerGang = 0)", [&] {
        std::vector<Entry> in = { {"neutral", 0} };
        const auto r = Deserialize(Serialize(in));
        REQUIRE(r.ok);
        REQUIRE_EQ(r.entries[0].ownerGang, 0);
    });

    t.run("round-trip: long territory id preserved", [&] {
        const std::string longId(200, 'x');
        std::vector<Entry> in = { {longId, 7} };
        const auto r = Deserialize(Serialize(in));
        REQUIRE(r.ok);
        REQUIRE_EQ(r.entries[0].id, longId);
    });

    // ------------------------------------------------------------------
    // v1 legacy read
    // ------------------------------------------------------------------
    t.run("v1 legacy: reads correctly", [&] {
        std::vector<Entry> in = { {"1001", 7}, {"1002", 8} };
        const auto blob = BuildV1Blob(in);
        const auto r = Deserialize(blob);
        REQUIRE(r.ok);
        REQUIRE_EQ((int)r.entries.size(), 2);
        REQUIRE_EQ(r.entries[0].id, std::string("1001"));
        REQUIRE_EQ(r.entries[0].ownerGang, 7);
        REQUIRE_EQ(r.entries[1].ownerGang, 8);
    });

    t.run("v1 legacy: empty entries", [&] {
        const auto blob = BuildV1Blob({});
        const auto r = Deserialize(blob);
        REQUIRE(r.ok);
        REQUIRE(r.entries.empty());
    });

    // ------------------------------------------------------------------
    // Error handling
    // ------------------------------------------------------------------
    t.run("error: empty blob", [&] {
        const auto r = Deserialize({});
        REQUIRE_FALSE(r.ok);
        REQUIRE_FALSE(r.error.empty());
    });

    t.run("error: wrong magic", [&] {
        auto bytes = Serialize({ {"1001", 7} });
        bytes[0] ^= 0xFF; // corrupt magic
        const auto r = Deserialize(bytes);
        REQUIRE_FALSE(r.ok);
    });

    t.run("error: truncated after header", [&] {
        std::vector<unsigned char> bytes(6, 0); // too short for full header
        const auto r = Deserialize(bytes);
        REQUIRE_FALSE(r.ok);
    });

    t.run("error: truncated mid-entry in v1", [&] {
        auto blob = BuildV1Blob({ {"1001", 7} });
        blob.resize(blob.size() - 2); // chop off last 2 bytes
        const auto r = Deserialize(blob);
        REQUIRE_FALSE(r.ok);
    });
}
