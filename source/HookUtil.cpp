#include "HookUtil.h"
#include <Windows.h>
#include <cstdint>
#include <cstring>

namespace HookUtil {

    bool WriteRelJmp(void* src, void* dst)
    {
        DWORD oldProtect{};
        if (!VirtualProtect(src, 5, PAGE_EXECUTE_READWRITE, &oldProtect))
            return false;

        auto* p = reinterpret_cast<std::uint8_t*>(src);
        p[0] = 0xE9; // JMP rel32
        std::int32_t rel = (std::int32_t)((std::uint8_t*)dst - ((std::uint8_t*)src + 5));
        std::memcpy(p + 1, &rel, sizeof(rel));

        VirtualProtect(src, 5, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), src, 5);
        return true;
    }

    void* MakeTrampoline(void* target, std::size_t stolenBytes)
    {
        void* tramp = VirtualAlloc(nullptr, stolenBytes + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!tramp) return nullptr;

        std::memcpy(tramp, target, stolenBytes);

        std::uint8_t* jmpFrom = (std::uint8_t*)tramp + stolenBytes;
        jmpFrom[0] = 0xE9;

        std::int32_t relBack = (std::int32_t)(((std::uint8_t*)target + stolenBytes) - (jmpFrom + 5));
        std::memcpy(jmpFrom + 1, &relBack, sizeof(relBack));

        FlushInstructionCache(GetCurrentProcess(), tramp, stolenBytes + 5);
        return tramp;
    }

    bool WriteRelCall(void* src, void* dst)
    {
        DWORD oldProt;
        if (!VirtualProtect(src, 5, PAGE_EXECUTE_READWRITE, &oldProt))
            return false;

        uint8_t* p = (uint8_t*)src;
        p[0] = 0xE8; // CALL rel32

        int32_t rel = (int32_t)((uint8_t*)dst - ((uint8_t*)src + 5));
        std::memcpy(p + 1, &rel, sizeof(rel));

        VirtualProtect(src, 5, oldProt, &oldProt);
        return true;
    }

} // namespace HookUtil
