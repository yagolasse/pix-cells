# pix-cells

A pixel art editor built in C++.

## Stack
- **Build**: CMake 3.20+, FetchContent
- **Window/Input**: SDL3 (static)
- **UI**: Dear ImGui — docking branch
- **Renderer**: OpenGL 3.3 core

## Build
```powershell
cmake -B build
cmake --build build
```

## Rules
- **Always build after every code change** to verify it compiles before reporting done.
- Run from `C:\Projects\pix-cells`.
- Executable: `build\Debug\pix-cells.exe` (or `build\pix-cells.exe` on single-config generators).

## Project layout
```
src/        source files
  main.cpp  entry point — SDL3 + ImGui init, main loop
```
