// main.cpp
#define GLFW_INCLUDE_NONE
#define IMGUI_IMPL_OPENGL_LOADER_GLAD

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "SystemMonitor.h"
#include "UI.h"
#include "types.h"

#include <thread>
#include <atomic>

extern bool popupVisible;
static bool requestOpenKillPopup = false;
int main() {
    glfwSetErrorCallback([](int e, const char* d) { fprintf(stderr, "GLFW %d: %s\n", e, d); });

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
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        fprintf(stderr, "GLAD init failed\n");
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    SystemMonitor monitor;
    std::atomic<bool> running{ true };
    KillAttempt lastKillAttempt;
    DWORD pendingKillPid = 0;

    std::thread updateThread([&]() {
        while (running) {
            monitor.Update();
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    });

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        UI::DrawSystemOverview(monitor);
        UI::DrawProcessesTable(monitor, lastKillAttempt, pendingKillPid, requestOpenKillPopup);
        UI::DrawKillConfirmation(lastKillAttempt);

        // Handle deferred popup request from UI
        if (requestOpenKillPopup) {
            ImGui::OpenPopup("ConfirmKill");
            popupVisible = true;
            requestOpenKillPopup = false;
        }

        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    running = false;
    if (updateThread.joinable()) updateThread.join();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}