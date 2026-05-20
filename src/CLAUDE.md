# src/ — Architecture

## Data model

All runtime state lives in `AppState` (`app_state.h`):

```
AppState
  CanvasState   canvas   — layers, composite buffer, zoom/pan, undo/redo stacks
  ToolsState    tools    — active_tool (0=Brush 1=Eraser 2=Fill 3=Line 4=Rect 5=Circle 6=Move),
                           brush_size, circle_brush, shape_filled
  PaletteState  palette  — primary_color, secondary_color (ImVec4 RGBA 0-1),
                           swatches (vector<ImVec4>), selected_swatch (int),
                           palette_name (string), recent_colors (vector<ImVec4>, max 8)

Layer (in layers vector)
  canvas (Canvas), name (string), visible (bool),
  locked (bool), opacity (float 0–1), blend_mode (int 0=Normal…4=Add)
```

### Pixel format
`uint32_t` RGBA8, **R in bits 0–7**, G 8–15, B 16–23, A 24–31.
Matches `GL_RGBA / GL_UNSIGNED_BYTE`. `ImGui::ColorConvertFloat4ToU32` produces this layout.

### CanvasState (`canvas_state.cpp`)
Owns `std::vector<Layer>` composited into a flat `composite` buffer for GPU upload.

Key methods:
| Method | What it does |
|--------|-------------|
| `active()` | Returns `Canvas&` for the active layer |
| `rebuild_composite()` | Porter-Duff "over" blend of visible layers → `composite`; applies each layer's `opacity` to source alpha before blending; sets `dirty=true` |
| `push_snapshot()` | Copies full layer stack onto `undo_stack`, clears `redo_stack` |
| `undo()` / `redo()` | Swaps layer stacks, calls `rebuild_composite()` |
| `new_canvas(w,h)` | Resets to single transparent 64×64 layer, clears undo/redo |

### Dirty flag
`cs.dirty = true` → `glTexSubImage2D` upload next frame (canvas_panel.cpp).
**Always call `cs.rebuild_composite()` after modifying any layer's pixels** — it sets dirty.

## File roles

| File | Role |
|------|------|
| `canvas.h` | `Canvas` struct: `width`, `height`, `pixels` (RGBA8 row-major); `set/get/fill/in_bounds` |
| `app_state.h` | All state structs: `Layer`, `CanvasState`, `ToolsState`, `PaletteState`, `AppState` |
| `app_state.cpp` | `PaletteState` constructor — initializes 24 pico-8 swatches, sets default primary/secondary/selected |
| `canvas_state.cpp` | `CanvasState` method implementations + `blend_over` (Porter-Duff) |
| `main.cpp` | SDL3+ImGui init, Mocha warm-dark theme, main loop, Ctrl+Z/Y undo/redo, B/E/F/L/R/C/M/[/] shortcuts |
| `workbench.h/cpp` | Fullscreen dockspace (`BeginWorkbench`), `EnsureDefaultLayout` (DockBuilder API) |
| `log.h/cpp` | `Log(fmt,...)` — writes to `pix-cells.log` + 500-entry in-memory ring buffer |
| `png_io.h/cpp` | `png_io::save(Canvas&, path)`, `png_io::load(Canvas&, path)` via stb_image |
