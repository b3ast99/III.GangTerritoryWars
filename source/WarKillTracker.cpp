#include "WarKillTracker.h"
#include <algorithm>

void WarKillTracker::Init() {
    m_records.clear();
}

void WarKillTracker::AddRecord(int gangType, const std::string& territoryId, unsigned int nowMs) {
    if ((int)m_records.size() >= kMaxRecords) {
        m_records.erase(m_records.begin()); // drop oldest
    }
    m_records.push_back({ gangType, territoryId, nowMs });
}

void WarKillTracker::PruneExpired(unsigned int nowMs, unsigned int windowMs) {
    auto it = m_records.begin();
    while (it != m_records.end()) {
        if ((nowMs - it->timestamp) > windowMs)
            it = m_records.erase(it);
        else
            ++it;
    }
}

int WarKillTracker::CountForTerritory(const std::string& territoryId, int gangType,
                                       unsigned int nowMs, unsigned int windowMs) const {
    int count = 0;
    for (const auto& r : m_records) {
        if ((nowMs - r.timestamp) <= windowMs &&
            r.territoryId == territoryId &&
            r.gangType    == gangType)
        {
            ++count;
        }
    }
    return count;
}
