#pragma once
#include "plugin.h"
#include "CFileMgr.h"

class TerritoryPersistence {
public:
    static void Init();
    static void Shutdown();
    static void Process(); // call every frame (gameProcessEvent)

private:
    static bool s_inited;
    static bool s_installed;

    // Trampolines
    using OpenFile_t = FILESTREAM(__cdecl*)(const char*, const char*);
    using CloseFile_t = int(__cdecl*)(FILESTREAM);
    static OpenFile_t  s_originalOpen;
    static CloseFile_t s_originalClose;

    static bool TryInstall();

    static FILESTREAM __cdecl OpenFileHook(const char* filePath, const char* mode);
    static int __cdecl CloseFileHook(FILESTREAM fileHandle);

private:
    // handle tracking (fixed arrays, safe inside hook)
    struct HandleOp { FILESTREAM h; int slot; bool isSave; bool isLoad; };
    static HandleOp s_ops[64];
    static bool s_used[64];

    static void Track(FILESTREAM h, int slot, bool isSave, bool isLoad);
    static bool Untrack(FILESTREAM h, HandleOp& out);

    static bool TryParseSaveSlotFromPath(const char* filePath, int& outSlot); // 1..8
    static bool IsReadMode(const char* mode);
    static bool IsWriteMode(const char* mode);

    static void OnSaveCompleted(int slot);
    static void OnLoadCompleted(int slot);

    // deferred IO (outside hook)
    static int s_pendingApplySlot;
    static int s_pendingWriteSlot;

    static void LoadSidecarAndApply(int slot);
    static void SaveSidecar(int slot);

    static const char* GetAsiDir();
    static void EnsureDirExists(const char* path);
};
