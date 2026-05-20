# src/ — Architecture

## Data model

All runtime state lives in `AppState` (`app_state.h`):

```
AppState
  CanvasState    canvas    — frames, composite buffer, zoom/pan, undo/redo stacks
  ToolsState     tools     — active_tool (0=Brush 1=Eraser 2=Fill 3=Line 4=Rect 5=FilledRect 6=Circle 7=FilledCircle 8=Move 9=RectSelect),
                             brush_size, circle_brush, shape_filled
  PaletteState   palette   — primary_color, secondary_color (ImVec4 RGBA 0-1),
                             swatches (vector<ImVec4>), selected_swatch (int),
                             palette_name (string), recent_colors (vector<ImVec4>, max 8)
  SelectionState selection — active (bool), x0/y0/x1/y1 (canvas pixels, top-left/bottom-right inclusive),
                             floating (bool), float_pixels/float_w/float_h/float_x/float_y/float_orig_x/float_orig_y (lifted pixel buffer),
                             clipboard (vector<uint32_t>), clipboard_w/h/ox/oy

Layer (in Frame.layers)
  canvas (Canvas), name (string), visible (bool),
  locked (bool), opacity (float 0–1), blend_mode (uint8_t 0=Normal…4=Add)

Frame (in CanvasState.frames)
  layers (vector<Layer>), duration_ms (uint16_t, default 100ms)
```

### Pixel format
`uint32_t` RGBA8, **R in bits 0–7**, G 8–15, B 16–23, A 24–31.
Matches `GL_RGBA / GL_UNSIGNED_BYTE`. `ImGui::ColorConvertFloat4ToU32` produces this layout.

### CanvasState (`canvas_state.cpp`)
Owns `std::vector<Frame>` (each with its own layer stack) composited into a flat `composite` buffer for GPU upload. `active_frame` and `active_layer` index the currently-edited canvas.

Key methods:
| Method | What it does |
|--------|-------------|
| `active()` | Returns `Canvas&` for `frames[active_frame].layers[active_layer]` |
| `active_layer_locked()` | Returns `bool` — true if the active layer has `locked=true`; use to guard all pixel-write operations |
| `active_layers()` | Returns `vector<Layer>&` for the active frame's layer stack |
| `rebuild_composite()` | Composites active frame's visible layers → `composite` applying per-layer opacity and blend mode (Normal/Multiply/Screen/Overlay/Add); sets `dirty=true` |
| `push_snapshot()` | Copies full `frames` vector onto `undo_stack`, clears `redo_stack` |
| `undo()` / `redo()` | Swaps `frames` vectors, calls `rebuild_composite()` |
| `new_canvas(w,h)` | Resets to one frame with one transparent layer, clears undo/redo |

### Dirty flag
`cs.dirty = true` → `glTexSubImage2D` upload next frame (canvas_panel.cpp).
**Always call `cs.rebuild_composite()` after modifying any layer's pixels** — it sets dirty.

## File roles

| File | Role |
|------|------|
| `canvas.h` | `Canvas` struct: `width`, `height`, `pixels` (RGBA8 row-major); `set/get/fill/in_bounds` |
| `app_state.h` | All state structs: `Layer`, `CanvasState`, `ToolsState`, `PaletteState`, `SelectionState`, `AppState` |
| `app_state.cpp` | `PaletteState` constructor — initializes 24 pico-8 swatches, sets default primary/secondary/selected |
| `canvas_state.cpp` | `CanvasState` method implementations + `blend_pixel` (Porter-Duff "over" with Multiply/Screen/Overlay/Add modes and per-layer opacity) |
| `main.cpp` | SDL3+ImGui init, main loop, Ctrl+Z/Y undo/redo, B/E/F/L/R/U/M/S/[/] tool shortcuts (U=Circle); Ctrl+A select-all, Ctrl+C/X copy/cut (locked layer blocked), Ctrl+V paste (locked layer blocked), Delete/Backspace erase selection (locked layer blocked), Escape deselect/cancel-float |
| `workbench.h/cpp` | Fullscreen dockspace (`BeginWorkbench`), `EnsureDefaultLayout` (DockBuilder API) |
| `log.h/cpp` | `Log(fmt,...)` — writes to `pix-cells.log` + 500-entry in-memory ring buffer |
| `png_io.h/cpp` | `png_io::save(Canvas&, path)`, `png_io::load(Canvas&, path)` via stb_image; `png_io::save_sprite_sheet(CanvasState&, path, SheetLayout, cols)` — composites each frame and blits into a single PNG |
| `pixc_io.h/cpp` | `pixc_io::save(AppState&, path)`, `pixc_io::load(AppState&, path)` — custom binary format (`PIXC` magic, version, frames + layers + pixels) |

## PIXC file format

Binary, little-endian (native byte order via `fread`/`fwrite`). Extension `.pixc`.

```
HEADER  (16 bytes)
  [0-3]   char[4]   magic         'P' 'I' 'X' 'C'
  [4-5]   uint16    version       currently 1
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
```

Notes:
- `ToolsState` and `PaletteState.selected_swatch` / `recent_colors` are **not** persisted.
- String encoding: 1-byte length prefix, then raw bytes, max 255 chars (`write_str` / `read_str` in `pixc_io.cpp`).
- On load, `active_frame` and `active_layer` reset to 0; `needs_center` is set so the canvas re-centers.
