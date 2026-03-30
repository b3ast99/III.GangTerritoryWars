#pragma once
// Stub: CModelInfo for unit tests — GetModelInfo always returns nullptr (model not found).
#include "CBaseModelInfo.h"
struct CModelInfo {
    static CBaseModelInfo* GetModelInfo(const char*, int* outIdx) {
        if (outIdx) *outIdx = -1;
        return nullptr;
    }
};
