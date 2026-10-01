#pragma once

#include <windows.h>

#include <cstdint>
#include <vector>

// Returns the remote addresses of the TP_CALLBACK_INSTANCE stack slots for
// every worker currently linked into the supplied TP_POOL.
//
// Private/build-specific implementation: Windows 10 19045 x64,
// ntdll.dll 10.0.19041.4842.
std::vector<uintptr_t> FindRemoteTpCallbackInstanceSlotsFromPool(
    HANDLE process, uintptr_t poolAddress);
