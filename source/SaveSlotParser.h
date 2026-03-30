#pragma once
// Pure utility functions for parsing GTA III save-file paths and open modes.
// No game engine dependencies — safe to include in unit test projects.

namespace SaveSlotParser {

// Returns true and sets outSlot (1..8) if filePath contains a GTA3sf{N}.b pattern.
bool TryParseSaveSlot(const char* filePath, int& outSlot);

// Returns true if mode is a read-only mode (contains 'r', no 'w' or 'a').
bool IsReadMode(const char* mode);

// Returns true if mode is a write mode (contains 'w' or 'a').
bool IsWriteMode(const char* mode);

} // namespace SaveSlotParser
