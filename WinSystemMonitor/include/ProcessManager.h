// ProcessManager.h
// Handles all Windows API calls related to processes (listing & termination)

#pragma once

#include "types.h"
#include <vector>

class ProcessManager {
public:
    // Get list of current running processes with name and memory usage
    bool EnumerateProcesses(std::vector<ProcessInfo>& outProcesses);

    // Try to terminate (kill) a process by its PID
    bool TerminateProcess(DWORD pid);
};