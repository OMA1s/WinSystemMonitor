// types.h
// Shared data structures used across the project

#pragma once

#include <windows.h>
#include <vector>
#include <string>
#include <array>

// Information about one running process
struct ProcessInfo {
    DWORD pid = 0;              // Unique process ID
    std::wstring name;          // Name of the process (supports Unicode)
    SIZE_T workingSet = 0;      // Memory usage in bytes (Working Set)
};

// Used to remember which process the user wants to kill
struct KillAttempt {
    DWORD pid = 0;
    std::wstring name;
};

// Number of samples kept for CPU/memory graphs (~4 minutes at 2s updates)
constexpr int HISTORY_SIZE = 120;