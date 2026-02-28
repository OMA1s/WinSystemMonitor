// ProcessManager.cpp
// Implementation of process listing and termination using Windows APIs

#include "ProcessManager.h"
#include <psapi.h>
#include <algorithm>

bool ProcessManager::EnumerateProcesses(std::vector<ProcessInfo>& outProcesses) {
    outProcesses.clear();

    DWORD pids[1024];
    DWORD bytesReturned;
    if (!EnumProcesses(pids, sizeof(pids), &bytesReturned)) {
        return false;
    }

    DWORD count = bytesReturned / sizeof(DWORD);

    for (DWORD i = 0; i < count; ++i) {
        DWORD pid = pids[i];
        if (pid == 0) continue;

        HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (!h) continue;

        WCHAR name[MAX_PATH] = L"<unknown>";
        GetModuleBaseNameW(h, nullptr, name, MAX_PATH);

        PROCESS_MEMORY_COUNTERS mem{};
        mem.cb = sizeof(mem);
        SIZE_T memory = 0;
        if (GetProcessMemoryInfo(h, &mem, sizeof(mem))) {
            memory = mem.WorkingSetSize;
        }

        outProcesses.push_back({ pid, std::wstring(name), memory });

        CloseHandle(h);
    }

    // Sort processes by memory usage (biggest first)
    std::sort(outProcesses.begin(), outProcesses.end(),
        [](const ProcessInfo& a, const ProcessInfo& b) {
        return a.workingSet > b.workingSet;
    });

    return true;
}

bool ProcessManager::TerminateProcess(DWORD pid) {
    // Safety: never try to kill PID 0 or our own process
    if (pid == 0 || pid == GetCurrentProcessId()) {
        return false;
    }

    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!h) return false;

    BOOL success = ::TerminateProcess(h, 1);
    CloseHandle(h);
    return success != FALSE;
}