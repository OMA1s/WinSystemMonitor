// main.cpp - Windows System Monitor
// A simple real-time tool that shows running processes, CPU usage, memory usage
// and lets you safely terminate (kill) processes
//
// Built with:
// - C++17
// - Dear ImGui (easy GUI)
// - GLFW + OpenGL (window and drawing)
// - GLAD (OpenGL functions)
// - Windows APIs (process info, CPU, memory)

#define GLFW_INCLUDE_NONE               // Tell GLFW: don't include OpenGL headers
#define IMGUI_IMPL_OPENGL_LOADER_GLAD   // Tell ImGui: we are using GLAD to load OpenGL

#include <GLFW/glfw3.h>                 // Window and input
#include <glad/glad.h>                  // Loads OpenGL functions
#include "imgui.h"                      // The GUI library
#include "imgui_impl_glfw.h"            // Connects ImGui to GLFW
#include "imgui_impl_opengl3.h"         // Connects ImGui to OpenGL 3

#include <windows.h>                    // Windows functions (processes, memory, etc.)
#include <psapi.h>                      // Process memory info
#include <pdh.h>                        // Performance counters (CPU usage)
#pragma comment(lib, "pdh.lib")         // Link PDH library automatically
#pragma comment(lib, "psapi.lib")       // Link PSAPI library automatically

#include <vector>                       // To store list of processes
#include <string>                       // For process names
#include <mutex>                        // To safely share data between threads
#include <thread>                       // Background thread for updates
#include <chrono>                       // For sleep timing
#include <atomic>                       // Safe boolean for thread control
#include <algorithm>                    // For sorting processes
#include <cstdio>                       // For printf debugging

// ────────────────────────────────────────────────
// 1. Global variables (shared between main thread and update thread)
// ────────────────────────────────────────────────

// Structure for each process we want to show
struct ProcessInfo {
    DWORD pid;              // Process ID number
    std::wstring name;      // Process name (wide string = supports Unicode)
    SIZE_T workingSet;      // How much memory the process is using (in bytes)
};

// List of all processes we found
std::vector<ProcessInfo> g_processes;

// Protects the list so main thread and update thread don't crash each other
std::mutex g_processesMutex;

// Tells the background thread when to stop
std::atomic<bool> g_running{ true };

// Which process the user clicked "Kill" on
struct KillAttempt {
    DWORD pid = 0;
    std::wstring name;
} lastKillAttempt;

// Used to open popup after table is finished (safer in ImGui tables)
DWORD g_pendingKillPid = 0;

// History for graphs (last 120 values ≈ 4 minutes when updating every 2 seconds)
constexpr int HISTORY_SIZE = 120;
float g_cpuHistory[HISTORY_SIZE] = { 0.0f };
int g_cpuHistoryPos = 0;

float g_memHistory[HISTORY_SIZE] = { 0.0f };
int g_memHistoryPos = 0;

// PDH handles for reading CPU usage
PDH_HQUERY g_cpuQuery = nullptr;
PDH_HCOUNTER g_cpuCounter = nullptr;

// ────────────────────────────────────────────────
// 2. Functions that run in background thread
// ────────────────────────────────────────────────

// Collects CPU % and memory % every update
void UpdateSystemMetrics() {
    // --- CPU usage ---
    if (g_cpuQuery && g_cpuCounter) {
        if (PdhCollectQueryData(g_cpuQuery) == ERROR_SUCCESS) {
            PDH_FMT_COUNTERVALUE value;
            if (PdhGetFormattedCounterValue(g_cpuCounter, PDH_FMT_DOUBLE, nullptr, &value) == ERROR_SUCCESS) {
                float cpu = static_cast<float>(value.doubleValue);
                if (cpu < 0) cpu = 0;
                if (cpu > 100) cpu = 100;
                g_cpuHistory[g_cpuHistoryPos] = cpu;
                g_cpuHistoryPos = (g_cpuHistoryPos + 1) % HISTORY_SIZE;
            }
        }
    }

    // --- Memory usage ---
    MEMORYSTATUSEX memInfo{};
    memInfo.dwLength = sizeof(memInfo);
    if (GlobalMemoryStatusEx(&memInfo)) {
        float usedPercent = 100.0f * (1.0f - static_cast<float>(memInfo.ullAvailPhys) / memInfo.ullTotalPhys);
        g_memHistory[g_memHistoryPos] = usedPercent;
        g_memHistoryPos = (g_memHistoryPos + 1) % HISTORY_SIZE;
    }
}

// This function runs in a separate thread and updates data every ~2 seconds
void UpdateProcessesThread() {
    while (g_running) {
        std::vector<ProcessInfo> newList;

        // Ask Windows for list of all process IDs
        DWORD pids[1024];
        DWORD bytesReturned;
        if (!EnumProcesses(pids, sizeof(pids), &bytesReturned)) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        DWORD count = bytesReturned / sizeof(DWORD);

        // For each process ID
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

            newList.push_back({ pid, std::wstring(name), memory });

            CloseHandle(h);
        }

        // Sort by memory usage (biggest first)
        std::sort(newList.begin(), newList.end(),
            [](const ProcessInfo& a, const ProcessInfo& b) {
            return a.workingSet > b.workingSet;
        });

        // Safely replace the global list
        {
            std::lock_guard<std::mutex> lock(g_processesMutex);
            g_processes = std::move(newList);
        }

        // Also update CPU and memory graphs
        UpdateSystemMetrics();

        // Wait before next update
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

// ────────────────────────────────────────────────
// 3. Main program
// ────────────────────────────────────────────────

int main() {
    // ── 3.1 Create window ────────────────────────────────────────

    glfwSetErrorCallback([](int err, const char* desc) {
        fprintf(stderr, "GLFW error %d: %s\n", err, desc);
    });

    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Windows System Monitor", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync on

    // Load OpenGL functions with GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        fprintf(stderr, "Failed to initialize GLAD\n");
        return 1;
    }

    // ── 3.2 Setup Dear ImGui ─────────────────────────────────────

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // ── 3.3 Setup CPU counter (PDH) ──────────────────────────────

    PdhOpenQueryW(nullptr, 0, &g_cpuQuery);
    PdhAddEnglishCounterW(g_cpuQuery, L"\\Processor(_Total)\\% Processor Time", 0, &g_cpuCounter);
    PdhCollectQueryData(g_cpuQuery);

    // ── 3.4 Start background update thread ───────────────────────

    std::thread updateThread(UpdateProcessesThread);

    // ── 3.5 Main loop ────────────────────────────────────────────

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ── System Overview window ───────────────────────────────
        ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(ImVec2(450, 600), ImGuiCond_Appearing);
        ImGui::Begin("System Overview");

        ImGui::Text("CPU: %.1f%%", g_cpuHistory[(g_cpuHistoryPos + HISTORY_SIZE - 1) % HISTORY_SIZE]);
        ImGui::PlotLines("##CPU", g_cpuHistory, HISTORY_SIZE, g_cpuHistoryPos,
            nullptr, 0.0f, 100.0f, ImVec2(0, 80));

        MEMORYSTATUSEX m{};
        m.dwLength = sizeof(m);
        GlobalMemoryStatusEx(&m);
        float usedGB = (m.ullTotalPhys - m.ullAvailPhys) / (1024.0f * 1024 * 1024);
        float totalGB = m.ullTotalPhys / (1024.0f * 1024 * 1024);

        ImGui::Text("Memory: %.1f / %.1f GB (%.1f%%)",
            usedGB, totalGB,
            100.0f * (1.0f - float(m.ullAvailPhys) / m.ullTotalPhys));

        ImGui::PlotLines("##Memory", g_memHistory, HISTORY_SIZE, g_memHistoryPos,
            nullptr, 0.0f, 100.0f, ImVec2(0, 80));

        ImGui::End();

        // ── Processes window ─────────────────────────────────────
        ImGui::SetNextWindowPos(ImVec2(500, 20), ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(ImVec2(600, 600), ImGuiCond_Appearing);
        ImGui::Begin("Processes");

        ImGui::Text("Running processes (updated ~every 2s) | FPS: %.1f", io.Framerate);

        {
            std::lock_guard<std::mutex> lock(g_processesMutex);

            if (ImGui::BeginTable("proc_table", 4,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
                ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 20))) {

                ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_WidthFixed, 80);
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Mem (MB)", ImGuiTableColumnFlags_WidthFixed, 100);
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 80);
                ImGui::TableHeadersRow();

                for (const auto& p : g_processes) {
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0); ImGui::Text("%lu", p.pid);
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%ws", p.name.c_str());
                    ImGui::TableSetColumnIndex(2); ImGui::Text("%.1f", float(p.workingSet) / (1024 * 1024));

                    ImGui::TableSetColumnIndex(3);
                    ImGui::PushID(static_cast<int>(p.pid));
                    if (ImGui::Button("Kill")) {
                        g_pendingKillPid = p.pid;
                        lastKillAttempt.name = p.name;
                    }
                    ImGui::PopID();
                }

                ImGui::EndTable();

                // Open popup after table is finished
                if (g_pendingKillPid != 0) {
                    lastKillAttempt.pid = g_pendingKillPid;
                    ImGui::OpenPopup("ConfirmKill");
                    g_pendingKillPid = 0;
                }
            }
        }

        // ── Kill confirmation popup ──────────────────────────────
        if (ImGui::BeginPopupModal("ConfirmKill", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Terminate process?");
            ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1),
                "%ws (PID: %lu)",
                lastKillAttempt.name.c_str(), lastKillAttempt.pid);

            ImGui::Separator();

            if (ImGui::Button("Yes, Terminate", ImVec2(140, 0))) {
                if (lastKillAttempt.pid != 0 && lastKillAttempt.pid != GetCurrentProcessId()) {
                    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, lastKillAttempt.pid);
                    if (h) {
                        TerminateProcess(h, 1);
                        CloseHandle(h);
                    }
                }
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel", ImVec2(140, 0))) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::Dummy(ImVec2(320, 0)); // minimum width
            ImGui::EndPopup();
        }

        ImGui::End(); // End Processes window

        // ── Rendering ────────────────────────────────────────────
        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // ── Cleanup ──────────────────────────────────────────────────
    g_running = false;
    if (updateThread.joinable()) updateThread.join();

    if (g_cpuQuery) PdhCloseQuery(g_cpuQuery);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}