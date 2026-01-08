#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <map>
#include <Windows.h>

class IniConfig {
public:
    static IniConfig& Instance() {
        static IniConfig instance;
        return instance;
    }

    // Get the directory where the calling module (ASI) is located
    static std::string GetModuleDirectory(HMODULE hModule = nullptr) {
        char path[MAX_PATH];
        if (hModule == nullptr) {
            // Get handle to this DLL
            MEMORY_BASIC_INFORMATION mbi;
            VirtualQuery(&GetModuleDirectory, &mbi, sizeof(mbi));
            hModule = (HMODULE)mbi.AllocationBase;
        }

        GetModuleFileNameA(hModule, path, MAX_PATH);

        // Remove filename, keep directory
        std::string fullPath(path);
        size_t lastSlash = fullPath.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            return fullPath.substr(0, lastSlash + 1);
        }
        return "";
    }

    void Load(const std::string& filename) {
        // Try to load from module directory first
        std::string modulePath = GetModuleDirectory() + filename;

        std::ifstream file(modulePath);
        if (!file.is_open()) {
            // Fallback to working directory
            file.open(filename);
            if (!file.is_open()) return;
        }

        std::string line, section;
        while (std::getline(file, line)) {
            // Remove whitespace
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);

            if (line.empty() || line[0] == ';' || line[0] == '#') continue;

            if (line[0] == '[') {
                section = line.substr(1, line.find(']') - 1);
            }
            else {
                size_t pos = line.find('=');
                if (pos != std::string::npos) {
                    std::string key = line.substr(0, pos);
                    std::string value = line.substr(pos + 1);
                    // Trim
                    key.erase(key.find_last_not_of(" \t") + 1);
                    value.erase(0, value.find_first_not_of(" \t"));

                    data_[section + "." + key] = value;
                }
            }
        }
    }

    int GetInt(const std::string& section, const std::string& key, int defaultValue) {
        auto it = data_.find(section + "." + key);
        if (it != data_.end()) {
            return std::stoi(it->second);
        }
        return defaultValue;
    }

    float GetFloat(const std::string& section, const std::string& key, float defaultValue) {
        auto it = data_.find(section + "." + key);
        if (it != data_.end()) {
            return std::stof(it->second);
        }
        return defaultValue;
    }

private:
    std::map<std::string, std::string> data_;
};