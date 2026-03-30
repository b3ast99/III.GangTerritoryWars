#pragma once
// Stub: DebugLog for unit tests — all writes are no-ops.
// Declarations only; implementations are in DebugLog_stub.cpp.
class DebugLog {
public:
    static void Initialize(const char* = "");
    static void Shutdown();
    static void Enable(bool);
    static void Write(const char*, ...);
    static void WritePedInfo(const char*, void*, int, float, float, float, float);
};
