#pragma once
#include <cstddef>
#include <cstdint>

namespace HookUtil {

	// Writes a 5-byte JMP rel32 at src -> dst. Returns false on failure.
	bool WriteRelJmp(void* src, void* dst);

	// Allocates a trampoline: copies stolen bytes from target, then appends a JMP back.
	void* MakeTrampoline(void* target, std::size_t stolenBytes);

} // namespace HookUtil
