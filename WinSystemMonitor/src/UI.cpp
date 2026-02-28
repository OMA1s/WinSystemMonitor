// UI.cpp
#include "UI.h"
#include "imgui.h"
#include "SystemMonitor.h"
#include "ProcessManager.h"

bool popupVisible = false;

namespace UI {

    void DrawSystemOverview(const SystemMonitor& monitor) {
        ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(ImVec2(450, 600), ImGuiCond_Appearing);
        ImGui::Begin("System Overview");

        float cpu = monitor.GetLatestCpu();
        ImGui::Text("CPU Usage: %.1f%%", cpu);
        ImGui::PlotLines("##CPU", monitor.GetCpuHistory().data(), HISTORY_SIZE,
            monitor.GetCpuHistoryPos(), nullptr, 0.0f, 100.0f, ImVec2(0, 80));

        MEMORYSTATUSEX m{};
        m.dwLength = sizeof(m);
        GlobalMemoryStatusEx(&m);
        float usedGB = (m.ullTotalPhys - m.ullAvailPhys) / (1024.0f * 1024 * 1024);
        float totalGB = m.ullTotalPhys / (1024.0f * 1024 * 1024);
        float memPct = monitor.GetLatestMemoryPercent();

        ImGui::Text("Memory: %.1f / %.1f GB (%.1f%%)", usedGB, totalGB, memPct);
        ImGui::PlotLines("##Memory", monitor.GetMemHistory().data(), HISTORY_SIZE,
            monitor.GetMemHistoryPos(), nullptr, 0.0f, 100.0f, ImVec2(0, 80));

        ImGui::End();
    }

    void DrawProcessesTable(SystemMonitor& monitor, KillAttempt& lastKill, DWORD& pendingKillPid, bool& requestOpenPopup) {
        ImGui::SetNextWindowPos(ImVec2(500, 20), ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(ImVec2(600, 600), ImGuiCond_Appearing);
        ImGui::Begin("Processes");

        ImGui::Text("Running processes (updated ~every 2s)");

        auto processes = monitor.GetProcesses();

        if (ImGui::BeginTable("proc_table", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
            ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 20))) {

            ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Mem (MB)", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableHeadersRow();

            for (const auto& p : processes) {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%lu", p.pid);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%ws", p.name.c_str());

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.1f", static_cast<float>(p.workingSet) / (1024.0f * 1024.0f));

                ImGui::TableSetColumnIndex(3);

                ImGui::PushID(static_cast<int>(p.pid));
                if (ImGui::Button("Kill")) {
                    pendingKillPid = p.pid;
                    lastKill.name = p.name;
                }
                ImGui::PopID();
            }

            ImGui::EndTable();

            if (pendingKillPid != 0) {
                lastKill.pid = pendingKillPid;
                requestOpenPopup = true;
                //ImGui::OpenPopup("ConfirmKill");
                popupVisible = true;
                pendingKillPid = 0;
            }
        }

        ImGui::End();
    }

    void DrawKillConfirmation(KillAttempt& attempt) {
        if (ImGui::BeginPopupModal("ConfirmKill", &popupVisible, 0)) {
            ImGui::Text("Terminate this process?");
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                "%ws (PID: %lu)", attempt.name.c_str(), attempt.pid);

            ImGui::Separator();

            if (ImGui::Button("Yes, Terminate", ImVec2(140, 0))) {
                ProcessManager mgr;
                mgr.TerminateProcess(attempt.pid);
                popupVisible = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel", ImVec2(140, 0))) {
                popupVisible = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

} // namespace UI