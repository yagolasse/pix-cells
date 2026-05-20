# src/panels/ — Panel responsibilities

Each panel is a self-contained ImGui window. Panels receive references only to the state they need.

| Panel | Signature | Responsibility |
|-------|-----------|----------------|
| `canvas_panel` | `DrawCanvas(CanvasState&, ToolsState&, PaletteState&)` | GL texture, checkerboard, zoom-to-cursor, tool input |
| `layers_panel` | `DrawLayers(CanvasState&)` | Layer list (top = highest index), add/delete/rename/visibility |
| `tools_panel` | `SetIconFont(ImFont*)`, `DrawTools(ToolsState&)` | Tool buttons (0=Brush 1=Eraser 2=Fill 3=Line 4=Rect 5=Circle 6=Move) rendered as FA icons via `PushFont`/`PopFont`; view toggles (symmetry, grid, onion skin); brush size slider |
| `palette_panel` | `SetPaletteIconFont(ImFont*)`, `DrawPalette(PaletteState&)` | "Color" window: current/previous swatch strip + swap, HSV/RGB/HEX tabs (picker, channel inputs, recent row), palette grid (8-col, selected outline), Add/Remove/Sort |
| `menu_bar` | `DrawMenuBar(AppState&, SDL_Window*, bool& show_log)` → bool | File (New/Open/Save), Edit (Undo/Redo/Canvas Settings), View (Log toggle), SDL3 async file dialogs |
| `log_panel` | `DrawLog(bool* p_open)` | Displays `LogEntries()` ring buffer; auto-scrolls, Clear button; hidden by default (`show_log=false` in `main.cpp`), `NoTitleBar` |

## canvas_panel internals
- Texture recreated when `cs.width()/height()` changes; updated via `glTexSubImage2D` when `cs.dirty`.
- **Sampler fix**: ImGui v1.92+ binds a LINEAR sampler before each draw. Inject `DrawCallback_SetSamplerNearest` / `DrawCallback_SetSamplerLinear` around `ImGui::Image()` to preserve `GL_NEAREST`.
- **Zoom-to-cursor**: on scroll, adjusts `cs.pan` so the canvas pixel under the mouse stays fixed: `pan.x = mouse.x - (mouse.x - base.x - pan.x) / old_zoom * new_zoom - base.x`.
- `was_painting` static tracks stroke continuity; `push_snapshot()` fires once on stroke start (not per pixel).
- `base` = `ImGui::GetCursorScreenPos()` before `SetCursorScreenPos(origin)` — used as the fixed anchor for pan math.

## Icon font
`tools_panel.cpp` and `palette_panel.cpp` each hold a `static ImFont* s_icon_font`. `main.cpp` loads `fonts/fa-solid-900.ttf` and passes the pointer to both `panels::SetIconFont()` and `panels::SetPaletteIconFont()` before the main loop. Wrap icon-only button labels in `PushFont(s_icon_font)` / `PopFont()`. Icon defines (`ICON_FA_*`) come from `vendor/icons_font_awesome/IconsFontAwesome6.h`. To add an icon to a button label use compile-time string concatenation: `ICON_FA_PENCIL "##id"`.

## Adding a new tool
1. Add a comment in `app_state.h` `ToolsState` documenting the new index.
2. Add an entry to `tool_defs[]` in `tools_panel.cpp` with the FA icon and tooltip.
3. Add a handler branch in `canvas_panel.cpp`:
   - Brush-like tools: inside `IsItemHovered` in the `else` branch.
   - Shape tools (click-drag): start inside `IsItemHovered`; commit in the `shape_dragging` block outside `IsItemHovered`. Add a preview in the `if (shape_dragging)` block.
4. Add a keyboard shortcut in `main.cpp` if desired.
