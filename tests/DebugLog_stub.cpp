// Stub implementations of DebugLog for unit tests — all no-ops.
#include "stubs/DebugLog.h"

void DebugLog::Initialize(const char*) {}
void DebugLog::Shutdown() {}
void DebugLog::Enable(bool) {}
void DebugLog::Write(const char*, ...) {}
void DebugLog::WritePedInfo(const char*, void*, int, float, float, float, float) {}
