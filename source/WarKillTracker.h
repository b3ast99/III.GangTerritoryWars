#pragma once
#include <vector>
#include <string>

// Pure kill-record management for gang war provocation tracking.
// No game engine dependencies — safe to include in unit test projects.

struct KillRecord {
    int          gangType    = 0;
    std::string  territoryId;
    unsigned int timestamp   = 0;
};

class WarKillTracker {
public:
    static constexpr unsigned int kDefaultWindowMs = 15000u;
    static constexpr int          kMaxRecords       = 100;

    // Clear all records.
    void Init();

    // Insert a kill record. If at capacity, the oldest record is dropped first.
    void AddRecord(int gangType, const std::string& territoryId, unsigned int nowMs);

    // Remove all records where (nowMs - timestamp) > windowMs.
    void PruneExpired(unsigned int nowMs, unsigned int windowMs);

    // Count records for the given territory+gang within the time window.
    int CountForTerritory(const std::string& territoryId, int gangType,
                          unsigned int nowMs, unsigned int windowMs) const;

    size_t Size()    const { return m_records.size(); }
    bool   IsEmpty() const { return m_records.empty(); }

private:
    std::vector<KillRecord> m_records;
};
