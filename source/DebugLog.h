// File: DebugLog.h
#pragma once
#include <string>
#include <fstream>
#include <ctime>

class DebugLog {
private:
    static std::ofstream m_file;
    static bool m_enabled;

public:
    static void Initialize(const char* filename = "GTAGangWars.log");
    static void Shutdown();
    static void Enable(bool enable);
    static void Write(const char* format, ...);
    static void WritePedInfo(const char* context, void* pedPtr, int pedHandle,
        float x, float y, float z, float health);
};