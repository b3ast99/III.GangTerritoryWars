#pragma once
// Stub: CStreaming for unit tests — all operations are no-ops.
enum eStreamingFlags : unsigned int {
    GAME_REQUIRED  = 0x02,
    KEEP_IN_MEMORY = 0x10,
};
struct CStreaming {
    static void RequestModel(int, unsigned int) {}
    static void LoadAllRequestedModels(bool) {}
};
