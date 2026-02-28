# Windows System Monitor

A lightweight real-time system monitor built in C++ using Dear ImGui, GLFW, GLAD, and Windows APIs.

## Features
- List of running processes (PID, name, memory usage)
- Real-time CPU and memory usage graphs
- Terminate processes with confirmation dialog
- Multi-threaded updates with clean shutdown

## Technologies
- C++17
- Dear ImGui (immediate-mode GUI)
- GLFW + OpenGL 3.3
- Windows APIs: EnumProcesses, PDH, GlobalMemoryStatusEx, TerminateProcess

## Build Instructions
1. Visual Studio 2022 (x64)
2. Dependencies: GLFW (prebuilt), GLAD, Dear ImGui (sources included)
3. Add include/lib paths and link: glfw3.lib;opengl32.lib;pdh.lib;psapi.lib
4. Build & run (Debug/Release)
