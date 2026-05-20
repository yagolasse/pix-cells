# src/panels/ — Panel responsibilities

Each panel is a self-contained ImGui window. Panels receive references only to the state they need.

| Panel | Signature | Responsibility |
|-------|-----------|----------------|
| `canvas_panel` | `DrawCanvas(CanvasState&, ToolsState&, PaletteState&, SelectionState&)` | GL texture, checkerboard, zoom-to-cursor, tool input, marching-ants overlay, floating selection move/scale, resize handles, layer-lock enforcement |
| `layers_panel` | `DrawLayers(CanvasState&)` | Layer list (top = highest index), add/delete/rename/visibility |
| `tools_panel` | `SetIconFont(ImFont*)`, `DrawTools(ToolsState&)` | Tool buttons (0=Brush 1=Eraser 2=Fill 3=Line 4=Rect 5=FilledRect 6=Circle 7=FilledCircle 8=Move 9=RectSelect) rendered as FA icons via `PushFont`/`PopFont`; view toggles (symmetry, grid, onion skin); context-sensitive options: brush size + shape toggle (Brush/Eraser), brush size only (Line) |
| `palette_panel` | `SetPaletteIconFont(ImFont*)`, `DrawPalette(PaletteState&)` | "Color" window: current/previous swatch strip + swap, HSV/RGB/HEX tabs (picker, channel inputs, recent row), palette grid (8-col, selected outline), Add/Remove/Sort |
| `menu_bar` | `DrawMenuBar(AppState&, SDL_Window*, bool& show_log)` → bool | File (New/Open/Save as PIXC, Export > PNG / Sprite Sheet), Edit (Undo/Redo/Canvas Settings), View (Log toggle), SDL3 async file dialogs; detects PIXC vs PNG on open by magic bytes |
| `timeline_panel` | `SetTimelineIconFont(ImFont*)`, `DrawTimeline(CanvasState&)` | Transport buttons (|< prev, play/pause, >| next) advance frames at `cs.fps`; playhead slider; horizontally-scrolling frame strip of 56×64 cards (frame-number badge, duration badge, accent border on active frame); clicking a card sets `cs.active_frame`; add-frame card appends a new blank frame |
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

## Icon font
`tools_panel.cpp`, `palette_panel.cpp`, `layers_panel.cpp`, and `timeline_panel.cpp` each hold a `static ImFont* s_icon_font`. `main.cpp` loads `fonts/fa-solid-900.ttf` and passes the pointer to `panels::SetIconFont()`, `panels::SetPaletteIconFont()`, `panels::SetLayersIconFont()`, and `panels::SetTimelineIconFont()` before the main loop. Tooltips inside a `PushFont(s_icon_font)` block must call `PopFont()` **before** `SetTooltip` and `PushFont(s_icon_font)` **after** to restore the icon font. Do not use `PushFont(nullptr)` — in ImGui v1.92+ it re-pushes the current font rather than switching to the default. Wrap icon-only button labels in `PushFont(s_icon_font)` / `PopFont()`. Icon defines (`ICON_FA_*`) come from `vendor/icons_font_awesome/IconsFontAwesome6.h`. To add an icon to a button label use compile-time string concatenation: `ICON_FA_PENCIL "##id"`.

## Adding a new tool
1. Add a comment in `app_state.h` `ToolsState` documenting the new index.
2. Add an entry to `tool_defs[]` in `tools_panel.cpp` with the FA icon and tooltip. Update loop bound.
3. Add a handler branch in `canvas_panel.cpp`:
   - Brush-like tools: inside `IsItemHovered` in the `else` branch.
   - Shape tools (click-drag): extend the `active_tool >= 3 && active_tool <= 7` range; start inside `IsItemHovered`; commit in the `shape_dragging` `else` block's switch; add preview in `if (shape_dragging)`.
   - Non-pixel tools (e.g. selection): add an `else if` branch after tool 8; handle commit in the `tools.active_tool == 9` branch of the release block (skips `rebuild_composite`).
4. Update `active_tool == 8` (Move) and `active_tool == 9` (RectSelect) checks in `canvas_panel.cpp` if inserting before them.
5. Add a keyboard shortcut in `main.cpp` if desired.
