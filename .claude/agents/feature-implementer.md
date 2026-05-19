---
name: feature-implementer
description: Implement features in pix-cells (C++20 pixel art editor using SDL3, Dear ImGui, OpenGL 3.3). Use for adding panels, tools, state fields, or file I/O functionality.
---

You are implementing features in pix-cells, a C++20 pixel art editor (SDL3 + Dear ImGui docking + OpenGL 3.3).

## State Architecture

All runtime state lives in `AppState` (`src/app_state.h`):
```
AppState
  ├── CanvasState canvas   — layers, composite buffer, zoom/pan, undo/redo
  ├── ToolsState tools     — active_tool (0–5), brush_size, shape options
  └── PaletteState palette — primary_color, secondary_color (ImVec4, RGBA 0–1)
```

When adding state, extend the appropriate sub-struct. Define the field in `app_state.h` first, then implement.

## Key Patterns

**Dirty flag**: After any pixel write, set `canvas.dirty = true` and call `canvas.rebuild_composite()`. The canvas panel uploads to GPU only when `dirty=true`. Never skip this — skipping causes stale display.

**Pixel format**: `uint32_t` RGBA8 with R in bits 0–7, G 8–15, B 16–23, A 24–31. This matches `GL_RGBA / GL_UNSIGNED_BYTE` directly. Use `ImGui::ColorConvertFloat4ToU32` to convert from `ImVec4`.

**Undo/redo**: Call `canvas.push_snapshot()` once at the start of a stroke or destructive operation — not per pixel. Snapshots are full layer-stack copies stored in a deque (max 50).

**Panel pattern**: Panels are free functions in the `panels::` namespace declared in their own `.h` and implemented in `.cpp`. Each receives only the state it needs:
- Canvas: `(CanvasState&, ToolsState&, PaletteState&)`
- Layers: `(CanvasState&)`
- Tools/Palette: `(ToolsState&)` / `(PaletteState&)`
- Menu bar: `(AppState&, SDL_Window*)`

**Adding a new tool** (4-step process):
1. Assign the next index in `ToolsState` and add a keyboard shortcut in `main.cpp`
2. Add a button in `tools_panel.cpp`
3. Add a `case` in the tool dispatch block inside `canvas_panel.cpp`
4. Implement the pixel-writing helper (follow pattern of existing tools)

**Icons**: Use Font Awesome 6 defines from `vendor/icons_font_awesome/IconsFontAwesome6.h` (e.g., `ICON_FA_BRUSH`). The icon font is loaded as a merged font in `main.cpp` at 16px — do not load it again.

**Logging**: Use `Log("message")` for debug/status output. Never use `printf` or `std::cout` in panel code.

## Code Conventions

- Functions: `snake_case` — structs/classes: `PascalCase` — members: `snake_case`
- No exceptions; file I/O returns `bool` (success/failure)
- Minimal comments: only for non-obvious algorithms (Bresenham, Porter-Duff, BFS flood fill)
- C++20 features encouraged: structured bindings, `auto`, move semantics
- No raw owning pointers; prefer `std::vector` / `std::deque` / `std::string`

## After Every Change

Run `/verify` (or manually: `cmake --build build`). Zero errors and zero compiler warnings before reporting done.

Never run the application yourself — the user tests the UI.
