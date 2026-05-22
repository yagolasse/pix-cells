# src/ — Architecture

## Data model

All runtime state lives in `AppState` (`app_state.h`):

```
AppState
  CanvasState    canvas        — frames, composite buffer, zoom/pan, undo/redo stacks, unsaved_changes (bool)
  std::string    project_path  — path to .pixc file (empty = untitled); set on successful load/save
  ToolsState     tools         — active_tool (0=Brush 1=Eraser 2=Fill 3=Line 4=Rect 5=FilledRect 6=Circle 7=FilledCircle 8=Move 9=RectSelect 10=ColorPicker; named `tool::` constants in app_state.h),
                                 brush_size, circle_brush, shape_filled, show_grid, symmetry, onion_skin, onion_skin_mode (0=Both 1=Previous 2=Next), mouse_over_canvas (bool)
  PaletteState   palette       — primary_color, secondary_color (ImVec4 RGBA 0-1),
                                 swatches (vector<ImVec4>), selected_swatch (int),
                                 palette_name (string), recent_colors (vector<ImVec4>, max 8)
  SelectionState selection     — active (bool), x0/y0/x1/y1 (canvas pixels, top-left/bottom-right inclusive;
                                 NOT clamped — may be negative or exceed canvas dimensions when dragged/scaled past the edge),
                                 floating (bool), float_pixels/float_w/float_h/float_x/float_y/float_orig_x/float_orig_y (lifted pixel buffer; float_x/y may be off-canvas),
                                 clipboard (vector<uint32_t>), clipboard_w/h/ox/oy

Layer (in Frame.layers)
  canvas (Canvas), name (string), visible (bool),
  locked (bool), opacity (float 0–1), blend_mode (uint8_t 0=Normal…4=Add)

Frame (in CanvasState.frames)
  layers (vector<Layer>), duration_ms (uint16_t, default 100ms)

AnimTag (in CanvasState.tags)
  name (string), start (int, inclusive), end (int, inclusive)
```

### Pixel format
`uint32_t` RGBA8, **R in bits 0–7**, G 8–15, B 16–23, A 24–31.
Matches `GL_RGBA / GL_UNSIGNED_BYTE`. `ImGui::ColorConvertFloat4ToU32` produces this layout.

### CanvasState (`canvas_state.cpp`)
Owns `std::vector<Frame>` (each with its own layer stack) composited into a flat `composite` buffer for GPU upload. `active_frame` and `active_layer` index the currently-edited canvas. **Invariant**: `active_layer` must always be a valid index into `frames[active_frame].layers`; code that switches frames or adds frames is responsible for clamping/resetting it (timeline_panel clamps on switch, resets to 0 on new-frame creation). `unsaved_changes` (bool) tracks whether the canvas has been modified since the last successful save; set to true by `push_snapshot()`, `undo()`, and `redo()`; reset to false by `new_canvas()` and when a file is successfully loaded or saved (in menu_bar.cpp).

Key methods:
| Method | What it does |
|--------|-------------|
| `active()` | Returns `Canvas&` for `frames[active_frame].layers[active_layer]` |
| `active_layer_locked()` | Returns `bool` — true if the active layer has `locked=true`; use to guard all pixel-write operations |
| `active_layers()` | Returns `vector<Layer>&` for the active frame's layer stack |
| `composite_frame(idx, out)` | Composites `frames[idx]`'s visible layers into `out` (resized to `width*height`); does not touch `composite` or `dirty` — used by the timeline thumbnail renderer |
| `rebuild_composite()` | Calls `composite_frame(active_frame, composite)`; sets `dirty=true` |
| `duplicate_frame(idx)` | `push_snapshot()`, inserts copy of `frames[idx]` after it, adjusts tag ranges, sets `active_frame`, `rebuild_composite()` |
| `delete_frame(idx)` | No-ops if only one frame; else `push_snapshot()`, erases, adjusts/clamps tag ranges and `active_frame`, `rebuild_composite()` |
| `push_snapshot()` | Pushes a `HistoryState` (full `frames` vector + `active_frame` + `active_layer`) onto `undo_stack`, clears `redo_stack` |
| `undo()` / `redo()` | Restores `frames` **and** `active_frame`/`active_layer` from the snapshot, calls `rebuild_composite()` — keeps the selection valid when the layer count changes |
| `new_canvas(w,h)` | Resets to one frame with one transparent layer, clears undo/redo |

### Dirty flag
`cs.dirty = true` → `glTexSubImage2D` upload next frame (canvas_panel.cpp).
**Always call `cs.rebuild_composite()` after modifying any layer's pixels** — it sets dirty.

## File roles

| File | Role |
|------|------|
| `canvas.h` | `Canvas` struct: `width`, `height`, `pixels` (RGBA8 row-major); `set/get/fill/in_bounds`. Both `set` and `get` are bounds-safe — `set` no-ops out-of-bounds, `get` returns `0x00000000` (transparent). This lets off-canvas selection regions be read/written without clipping at call sites. |
| `app_state.h` | All state structs: `Layer`, `CanvasState`, `ToolsState`, `PaletteState`, `SelectionState`, `AppState` |
| `app_state.cpp` | `PaletteState` constructor — initializes 24 pico-8 swatches, sets default primary/secondary/selected |
| `canvas_state.cpp` | `CanvasState` method implementations (uses `blend_pixel` from `blend.h`) |
| `blend.h` | `blend_pixel(dst, src, mode)` — Porter-Duff "over" with Multiply/Screen/Overlay/Add modes and per-layer opacity (RGBA8, R in bits 0–7); shared by `canvas_state.cpp` compositing and `png_io.cpp` sprite-sheet export |
| `raster.h/cpp` | `namespace raster` pure pixel-drawing algorithms operating on `Canvas&` (no ImGui/GL): `paint_pixel`, `bresenham`, `flood_fill`, `draw_rect`, `draw_ellipse`, `nn_scale`, and the `rasterize_ellipse(x0,y0,x1,y1,filled,plot)` template (header-only, emits points via a `plot` callback; shared by `draw_ellipse` and the canvas shape preview). Linked into `pix-cells-core` so tests can exercise them |
| `palette_io.h/cpp` | `namespace palette_io` — `load_hex`/`save_hex` (`.hex`/`.txt`, one `#RRGGBB` per line with `; name` comment header) and `load_gpl`/`save_gpl` (GIMP Palette `.gpl`, `GIMP Palette` magic + `Name:` + `R G B` color lines). All four take/return `PaletteState&` / `const PaletteState&` and return `bool`; log via `Log()`. Linked into `pix-cells-core`. |
| `icon_manager.h/cpp` | `icon_manager::init(dir)`, `::get(name)` → `ImTextureID`, `::shutdown()` — lazily loads SVGs from `icons/` via lunasvg (32×32, ARGB→RGBA swizzle + un-premultiply), uploads as GL textures, caches by name; missing files return texture 0 |
| `cursor_manager.h/cpp` | `cursor_manager::init(dir)`, `::set_for_tool(tool_index, mouse_pressed, mouse_over_canvas)`, `::shutdown()` — loads `point_scan.svg`, `eyedropper.svg`, `pan_tool.svg`, `pan_tool_alt.svg` as SDL color cursors; when `mouse_over_canvas` is false, uses the default system cursor; when true, tool 8 (Move) uses pan_tool / pan_tool_alt based on `mouse_pressed`, tool 10 (Color Picker) uses eyedropper, all others use point_scan |
| `main.cpp` | SDL3+ImGui init, main loop, Ctrl+Z/Y undo/redo, B/E/F/L/R/U/M/S/[/] tool shortcuts; R cycles Rect↔FilledRect (4↔5), U cycles Circle↔FilledCircle (6↔7); Ctrl+A select-all, Ctrl+C/X copy/cut (locked layer blocked), Ctrl+V paste (locked layer blocked), Delete/Backspace erase selection (locked layer blocked), Escape deselect/cancel-float |
| `workbench.h/cpp` | Fullscreen dockspace (`BeginWorkbench`) — calls `BuildDefaultLayout` automatically on first launch (when no ini node exists) before registering the DockSpace; docks 5 panels: Tools, Canvas, Color, Layers, Timeline; Log floats freely when toggled |
| `log.h/cpp` | `Log(fmt,...)` — writes to `pix-cells.log` + 500-entry in-memory ring buffer |
| `png_io.h/cpp` | `png_io::save(Canvas&, path)`, `png_io::load(Canvas&, path)` via stb_image; `png_io::save_sprite_sheet(CanvasState&, path, SheetLayout, cols)` — composites each frame and blits into a single PNG |
| `pixc_io.h/cpp` | `pixc_io::save(AppState&, path)`, `pixc_io::load(AppState&, path)` — custom binary format (`PIXC` magic, version, frames + layers + pixels) |

## PIXC file format

Binary, little-endian (native byte order via `fread`/`fwrite`). Extension `.pixc`.

```
HEADER  (16 bytes)
  [0-3]   char[4]   magic         'P' 'I' 'X' 'C'
  [4-5]   uint16    version       2 (v1 = no TAGS block)
  [6-7]   uint16    width         canvas width in pixels
  [8-9]   uint16    height        canvas height in pixels
  [10-11] uint16    frame_count
  [12-15] float32   fps

PALETTE
  uint16    color_count
  per color (×color_count):
    float32   r
    float32   g
    float32   b
    float32   a           (all channels 0–1)
  uint8     name_len
  char[name_len]  palette_name

FRAMES  (×frame_count)
  uint16    duration_ms
  uint16    layer_count
  per layer (×layer_count):
    uint8     name_len
    char[name_len]  layer_name
    uint8     visible     1=true, 0=false
    uint8     locked      1=true, 0=false
    float32   opacity     0–1
    uint8     blend_mode  0=Normal 1=Multiply 2=Screen 3=Overlay 4=Add
    uint32[width × height]  pixels   RGBA8, R in bits 0–7, row-major

TAGS  (version 2+ only)
  uint16    tag_count
  per tag (×tag_count):
    uint8     name_len
    char[name_len]  tag_name
    uint16    start       inclusive frame index
    uint16    end         inclusive frame index
  int16     active_tag   -1 = all frames
```

Notes:
- `ToolsState` and `PaletteState.selected_swatch` / `recent_colors` are **not** persisted.
- String encoding: 1-byte length prefix, then raw bytes, max 255 chars (`write_str` / `read_str` in `pixc_io.cpp`).
- On load, `active_frame` and `active_layer` reset to 0; `needs_center` is set so the canvas re-centers.
- Version 1 files load cleanly with no tags and `active_tag = -1`.
