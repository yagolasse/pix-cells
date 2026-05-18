# pix-cells

A pixel art editor built in C++.

## Stack
- **Build**: CMake 3.20+, FetchContent
- **Window/Input**: SDL3 (static)
- **UI**: Dear ImGui — docking branch (v1.92+)
- **Renderer**: OpenGL 3.3 core

## Build
```powershell
cmake -B build
cmake --build build
```

## Rules
- **Always build after every code change** to verify it compiles before reporting done.
- Run from `C:\Projects\pix-cells`.
- Executable: `build\Debug\pix-cells.exe`

## Architecture
- `src/CLAUDE.md` — data model, file roles, pixel format, dirty-flag pattern
- `src/panels/CLAUDE.md` — panel signatures, canvas_panel internals, how to add a tool
