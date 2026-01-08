// File: DebugLog.cpp
#include "DebugLog.h"
#include <cstdarg>
#include <windows.h>  // For OutputDebugStringA

std::ofstream DebugLog::m_file;
bool DebugLog::m_enabled = true;

void DebugLog::Initialize(const char* filename) {
    m_file.open(filename, std::ios::out | std::ios::trunc);
    if (m_file.is_open()) {
        Write("=== Gang Territory Wars Debug Log ===\n");
        Write("Log started at %s\n", __TIMESTAMP__);
    }
    else {
        OutputDebugStringA("Failed to open debug log file!");
    }
}

void DebugLog::Shutdown() {
    if (m_file.is_open()) {
        Write("=== Log ended ===\n");
        m_file.close();
    }
}

void DebugLog::Enable(bool enable) {
    m_enabled = enable;
}

void DebugLog::Write(const char* format, ...) {
    if (!m_enabled || !m_file.is_open()) return;

    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // Add timestamp
    time_t now = time(nullptr);
    tm timeinfo;
    localtime_s(&timeinfo, &now);

    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "[%H:%M:%S] ", &timeinfo);

    m_file << timestamp << buffer << std::endl;
    m_file.flush();  // Ensure immediate write

    // Also output to debugger
    OutputDebugStringA(buffer);
    OutputDebugStringA("\n");
}

void DebugLog::WritePedInfo(const char* context, void* pedPtr, int pedHandle,
    float x, float y, float z, float health) {
    Write("%s: Ped=%p Handle=%d Pos=(%.1f, %.1f, %.1f) Health=%.1f",
        context, pedPtr, pedHandle, x, y, z, health);
}