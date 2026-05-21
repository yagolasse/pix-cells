# src/panels/ — Panel responsibilities

Each panel is a self-contained ImGui window. Panels receive references only to the state they need.

| Panel | Signature | Responsibility |
|-------|-----------|----------------|
| `canvas_panel` | `DrawCanvas(CanvasState&, ToolsState&, PaletteState&, SelectionState&)` | GL texture, checkerboard, zoom-to-cursor, tool input, marching-ants overlay, floating selection move/scale, resize handles, layer-lock enforcement |
| `layers_panel` | `DrawLayers(CanvasState&)` | Layer list (top = highest index), add/delete/rename/visibility |
| `tools_panel` | `DrawTools(ToolsState&)` | Tool buttons (0=Brush 1=Eraser 2=Fill 3=Line 4=Rect 5=FilledRect 6=Circle 7=FilledCircle 8=Move 9=RectSelect 10=ColorPicker) rendered as SVG textures via `icon_manager`; view toggles (symmetry, grid, onion skin) with dual on/off icons for grid; context-sensitive options: brush size + shape toggle (Brush/Eraser), brush size only (Line) |
| `palette_panel` | `DrawPalette(PaletteState&)` | "Color" window: current/previous swatch strip + swap, HSV/RGB/HEX tabs (picker, channel inputs, recent row), palette grid (8-col, selected outline), Add/Remove/Sort |
| `menu_bar` | `DrawMenuBar(AppState&, SDL_Window*, bool& show_log)` → bool | File (New/Open/Save as PIXC, Export > PNG / Sprite Sheet), Edit (Undo/Redo/Canvas Settings), View (Log toggle), SDL3 async file dialogs; detects PIXC vs PNG on open by magic bytes |
| `timeline_panel` | `DrawTimeline(CanvasState&)` | Transport buttons (|< prev, play/pause, >| next) advance frames at `cs.fps`; playhead slider; horizontally-scrolling frame strip of 56×64 cards (frame-number badge, duration badge, accent border on active frame); clicking a card sets `cs.active_frame`; add-frame card appends a new blank frame |
| `log_panel` | `DrawLog(bool* p_open)` | Displays `LogEntries()` ring buffer; auto-scrolls, Clear button; `p_open` wires the window close button to the View > Log toggle |

## canvas_panel internals
- Texture recreated when `cs.width()/height()` changes; updated via `glTexSubImage2D` when `cs.dirty`.
- **Sampler fix**: ImGui v1.92+ binds a LINEAR sampler before each draw. Inject `DrawCallback_SetSamplerNearest` / `DrawCallback_SetSamplerLinear` around `ImGui::Image()` to preserve `GL_NEAREST`.
- **Zoom-to-cursor**: on scroll, adjusts `cs.pan` so the canvas pixel under the mouse stays fixed: `pan.x = mouse.x - (mouse.x - base.x - pan.x) / old_zoom * new_zoom - base.x`.
- `was_painting` static tracks stroke continuity; `push_snapshot()` fires once on stroke start (not per pixel).
- `base` = `ImGui::GetCursorScreenPos()` before `SetCursorScreenPos(origin)` — used as the fixed anchor for pan math.
- **Layer lock**: all pixel-write paths (brush, eraser, fill, shapes, cut/paste/delete) check `cs.active_layer_locked()` and log "Layer locked" without modifying the canvas.
- **Selection overlay**: when `sel.active`, two `AddRect` calls (alternating white/black at 8 Hz via `ImGui::GetTime()`) draw marching ants over the selected region.
- **Floating selection**: clicking inside an active selection with tool 9 calls `lift_selection()` (snapshot + erase pixels + store in `sel.float_pixels`); the lifted pixels render as `AddRectFilled` quads over the composite while floating. Mouse release keeps the float in place (it stays "in the air"); `commit_floating()` is called on tool switch or when the user clicks outside the float rect. Escape undoes the lift via `app.canvas.undo()` and clears `sel.active`.
- **Resize handles**: 8 `AddRectFilled` handles (TL/TM/TR/RM/BR/BM/BL/LM) drawn around `sel` when tool 9 is active. Dragging a handle resizes `sel.x0/y0/x1/y1`; on release, if floating, `nn_scale` (nearest-neighbour) scales `sel.float_pixels` to the new dimensions.
- **Tool 9 click priority**: (1) handle hit → handle drag, (2) inside floating rect → re-drag float, (3) inside sel rect + unlocked → lift, (4) elsewhere → new selection drag (clears `sel.active`).
- **Shape preview**: while `shape_dragging`, the preview overlay uses the same pixel-exact algorithms as the commit path (inline Bresenham + stamp for line, row/col loops for rect, dual-scan with `round()` for ellipse) — each canvas pixel renders as an `AddRectFilled` quad of size `cs.zoom`. Tool 9 (select) keeps a vector marching-ants overlay instead. Holding Shift during a rect/circle drag constrains the bounding box to a square (applied to both preview and commit via `epx`/`epy` effective endpoint).

## Icons
Icons are SVG files in `icons/` rasterized at runtime via lunasvg. `icon_manager::init("icons")` is called in `main.cpp` after `ImGui_ImplOpenGL3_Init`; `icon_manager::shutdown()` is called before `ImGui_ImplOpenGL3_Shutdown`. Panels call `icon_manager::get("name")` to retrieve an `ImTextureID` (GL texture, 32×32 RGBA, lazily loaded and cached). All icon buttons use `ImGui::ImageButton` with `ImGuiStyleVar_FramePadding` set to `(FP, FP)` (tools panel uses `FP=4` with `ICON=22`). Missing SVG files return texture ID 0 (blank button, no crash). SVG files use the naming convention `name.svg` / `name_filled.svg` / `name_off.svg` / `name_open.svg`.

SDL cursors are managed by `cursor_manager` (`src/cursor_manager.h/cpp`). `set_for_tool(tool_index, mouse_pressed)` is called each frame in `main.cpp`. Move tool (8) uses `pan_tool.svg` / `pan_tool_alt.svg` depending on `mouse_pressed`; Color Picker (10) uses `eyedropper.svg`; all others use `point_scan.svg`.

## Adding a new tool
1. Add a comment in `app_state.h` `ToolsState` documenting the new index.
2. Add an entry to `tool_defs[]` in `tools_panel.cpp` with the SVG icon name, `##tN` id, and tooltip. Place the corresponding `name.svg` in `icons/`.
3. Add a handler branch in `canvas_panel.cpp`:
   - Brush-like tools: inside `IsItemHovered` in the `else` branch.
   - Shape tools (click-drag): extend the `active_tool >= 3 && active_tool <= 7` range; start inside `IsItemHovered`; commit in the `shape_dragging` `else` block's switch; add a pixel-exact preview branch in `if (shape_dragging)` using `draw_px` / `stamp` lambdas (see existing t==3/4/5/6/7 branches for the pattern).
   - Non-pixel tools (e.g. selection): add an `else if` branch after tool 8; handle commit in the `tools.active_tool == 9` branch of the release block (skips `rebuild_composite`).
4. Update `active_tool == 8` (Move) and `active_tool == 9` (RectSelect) checks in `canvas_panel.cpp` if inserting before them.
5. Add a keyboard shortcut in `main.cpp` if desired.
