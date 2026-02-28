// SystemMonitor.cpp
// Implementation of system monitoring logic

#include "SystemMonitor.h"
#include <pdh.h>
#include <windows.h>

SystemMonitor::SystemMonitor() {
    // Initialize PDH for CPU usage
    PdhOpenQueryW(nullptr, 0, &m_cpuQuery);
    PdhAddEnglishCounterW(m_cpuQuery,
        L"\\Processor(_Total)\\% Processor Time",
        0, &m_cpuCounter);
    PdhCollectQueryData(m_cpuQuery);
}

SystemMonitor::~SystemMonitor() {
    if (m_cpuQuery) {
        PdhCloseQuery(m_cpuQuery);
    }
}

void SystemMonitor::Update() {
    // Refresh process list
    std::vector<ProcessInfo> temp;
    if (m_processMgr.EnumerateProcesses(temp)) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_processes = std::move(temp);
    }

    // Refresh CPU & memory
    UpdateSystemMetrics();
}

void SystemMonitor::UpdateSystemMetrics() {
    // CPU usage
    if (m_cpuQuery && m_cpuCounter) {
        if (PdhCollectQueryData(m_cpuQuery) == ERROR_SUCCESS) {
            PDH_FMT_COUNTERVALUE val;
            if (PdhGetFormattedCounterValue(m_cpuCounter, PDH_FMT_DOUBLE, nullptr, &val) == ERROR_SUCCESS) {
                float cpu = static_cast<float>(val.doubleValue);
                if (cpu < 0) cpu = 0;
                if (cpu > 100) cpu = 100;

                std::lock_guard<std::mutex> lock(m_mutex);
                m_cpuHistory[m_cpuHistoryPos] = cpu;
                m_cpuHistoryPos = (m_cpuHistoryPos + 1) % HISTORY_SIZE;
            }
        }
    }

    // Memory usage
    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        float pct = 100.0f * (1.0f - static_cast<float>(mem.ullAvailPhys) / mem.ullTotalPhys);

        std::lock_guard<std::mutex> lock(m_mutex);
        m_memHistory[m_memHistoryPos] = pct;
        m_memHistoryPos = (m_memHistoryPos + 1) % HISTORY_SIZE;
    }
}

std::vector<ProcessInfo> SystemMonitor::GetProcesses() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_processes;
}

float SystemMonitor::GetLatestCpu() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cpuHistory[(m_cpuHistoryPos + HISTORY_SIZE - 1) % HISTORY_SIZE];
}

float SystemMonitor::GetLatestMemoryPercent() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_memHistory[(m_memHistoryPos + HISTORY_SIZE - 1) % HISTORY_SIZE];
}

const std::array<float, HISTORY_SIZE>& SystemMonitor::GetCpuHistory() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cpuHistory;
}

int SystemMonitor::GetCpuHistoryPos() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cpuHistoryPos;
}

const std::array<float, HISTORY_SIZE>& SystemMonitor::GetMemHistory() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_memHistory;
}

int SystemMonitor::GetMemHistoryPos() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_memHistoryPos;
}