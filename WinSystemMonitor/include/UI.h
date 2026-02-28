// UI.h
// All ImGui drawing functions

#pragma once

#include "types.h"
#include "SystemMonitor.h"
#include <Windows.h>

class SystemMonitor; // forward declaration

extern bool popupVisible;
namespace UI {
    // Draw window with CPU and memory graphs
    void DrawSystemOverview(const SystemMonitor& monitor);

    // Draw table of running processes + kill buttons
    void DrawProcessesTable(SystemMonitor& monitor, KillAttempt& lastKill, DWORD& pendingKillPid, bool& requestOpenPopup);

    // Show confirmation dialog when user clicks Kill
    void DrawKillConfirmation(KillAttempt& attempt);
}