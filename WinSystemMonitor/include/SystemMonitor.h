// SystemMonitor.h
// Manages system metrics (CPU, memory) and process list

#pragma once

#include "types.h"
#include "ProcessManager.h"
#include <array>
#include <mutex>
#include <pdh.h>

class SystemMonitor {
public:
    SystemMonitor();
    ~SystemMonitor();

    // Update all data (called every ~2 seconds)
    void Update();

    // Thread-safe access to data
    std::vector<ProcessInfo> GetProcesses() const;
    float GetLatestCpu() const;
    float GetLatestMemoryPercent() const;
    const std::array<float, HISTORY_SIZE>& GetCpuHistory() const;
    int GetCpuHistoryPos() const;
    const std::array<float, HISTORY_SIZE>& GetMemHistory() const;
    int GetMemHistoryPos() const;

private:
    mutable std::mutex m_mutex;             // Protects shared data

    std::vector<ProcessInfo> m_processes;   // Current list of processes
    ProcessManager m_processMgr;            // Helper for process API calls

    // History buffers for graphs
    std::array<float, HISTORY_SIZE> m_cpuHistory{};
    int m_cpuHistoryPos = 0;
    std::array<float, HISTORY_SIZE> m_memHistory{};
    int m_memHistoryPos = 0;

    // PDH handles for CPU usage
    PDH_HQUERY m_cpuQuery = nullptr;
    PDH_HCOUNTER m_cpuCounter = nullptr;

    // Update CPU and memory values
    void UpdateSystemMetrics();
};