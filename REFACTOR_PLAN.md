# pix-cells Refactor & Improvement Plan

Audit date: 2026-05-28. 35 findings across 7 categories.

---

## Priority Order

1. Timeline thumbnail caching (HIGH perf — likely causing frame drops with animation)
2. File load input validation in `pixc_io.cpp` (safety — untrusted input)
3. `raster.cpp` integer overflow on large canvases (correctness)
4. Split `canvas_panel.cpp` (maintainability — blocks other cleanups)
5. Round-trip file I/O tests (test coverage — high value, low risk)

---

## 1. Memory Leaks / Resource Safety

| Severity | File | Finding |
|---|---|---|
| HIGH | `src/pixc_io.cpp:140-141` | `fread` return values unchecked — truncated files silently continue with uninitialized data |
| MEDIUM | `src/ui_scale.cpp:44` | `SDL_GetPrefPath` result not freed in all error paths |
| MEDIUM | `src/icon_manager.cpp:60` | `renderToBitmap` width/height never validated before texture upload |

---

## 2. Code Smells / Refactors

| Severity | File | Finding |
|---|---|---|
| MEDIUM | `src/input_handler.cpp:88-140` | Copy and Cut share identical selection-iteration loops — extract `fill_clipboard()` helper |
| MEDIUM | `src/panels/canvas_panel.cpp:447-463` | Symmetry mirroring logic duplicated between preview and commit paths |
| MEDIUM | `src/panels/canvas_panel.cpp:70-78` | Checkerboard drawn as O(rows×cols) rects per frame — should use a tiled texture instead |
| LOW | `src/panels/canvas_panel.cpp:668-690` | `update_selection_from_float()` duplicated twice |
| LOW | `src/panels/canvas_panel.cpp:509-516` | Color picker tool (tool 10) is a hardcoded branch instead of following the tool pattern |
| LOW | `src/ui_scale.cpp:22` | Magic `99.f` sentinel should be `std::numeric_limits<float>::max()` |

---

## 3. Architecture / Folder Restructuring

| Severity | File | Finding |
|---|---|---|
| MEDIUM | `src/panels/canvas_panel.cpp` | 756 lines — split overlays, tool input, and decoration into `canvas_overlay.cpp`, `canvas_tools.cpp` |
| MEDIUM | `src/panels/canvas_panel.cpp:29-37` | `DocRenderState` is private but could be shared with timeline/preview — move to a header |
| MEDIUM | `src/panels/menu_bar.cpp:23` + `palette_panel.cpp:23` | `static PendingIO` scattered across panels — centralize into `file_io_context.h` |
| LOW | `src/canvas_state.cpp:140-162` | `lift_selection()`/`commit_floating()` only used in canvas_panel — move to `selection.cpp` |

---

## 4. Test Coverage Gaps

| Severity | File | Finding |
|---|---|---|
| MEDIUM | `src/raster.cpp` | `color_select()` and `nn_scale()` have zero test coverage — edge cases untested |
| MEDIUM | `src/pixc_io.cpp`, `palette_io.cpp`, `png_io.cpp` | No round-trip save/load regression tests for any file format |
| MEDIUM | `src/blend.h:27-35` | Blend modes (Multiply, Screen, Overlay, Add) have no unit tests |
| MEDIUM | `src/raster.cpp:95-101` | Complex selection masks (disconnected regions, thin lines) untested |

---

## 5. Performance

| Severity | File | Finding |
|---|---|---|
| HIGH | `src/panels/timeline_panel.cpp:128-131` | Per-frame `composite_frame()` called for every visible thumbnail — O(frames × pixels × layers) |
| MEDIUM | `src/panels/canvas_panel.cpp:296-304` | Onion skin recomposited every frame even when nothing changed |
| MEDIUM | `src/panels/canvas_panel.cpp:128-157` | Marching ants outline regenerated every frame — pre-generate outline geometry |
| MEDIUM | `src/blend.h:7-35` | Per-pixel float division in compositing hot path — consider SIMD or LUTs |
| LOW | `src/input_handler.cpp:101,121` | `clipboard.resize()` called even when size unchanged |
| LOW | `src/panels/layers_panel.cpp:52` | Minor: string concat with `+` instead of reserve/`std::to_string` pre-alloc |

---

## 6. Safety & Correctness

| Severity | File | Finding |
|---|---|---|
| HIGH | `src/raster.cpp:63` | `w * h` used as `size_t` — 65535×65535 canvas overflows on 32-bit `size_t` |
| MEDIUM | `src/panels/canvas_panel.cpp:679-682` | `k_hmoves[active_handle]` — no bounds check; corrupted handle value reads past array |
| MEDIUM | `src/raster.cpp:130-133` | `nn_scale()` has no guard against `dw == 0` or `dh == 0` — division by zero |
| MEDIUM | `src/pixc_io.cpp:113-134` | Canvas dimensions from file never validated — zero or 65535×65535 not rejected before allocation |
| LOW | `src/panels/canvas_panel.cpp:135-137` | Signed `sel.x0/y0` used in float arithmetic without explicit cast |
| LOW | `src/app_state.cpp:21` | `hex_to_imvec4()` silently forces alpha=1.0f — alpha handling undocumented |
| LOW | `src/cursor_manager.cpp:38` | `SDL_CreateColorCursor` failure not logged — silent invisible cursor |

---

## 7. Other / Correctness

| Severity | File | Finding |
|---|---|---|
| MEDIUM | `src/panels/canvas_panel.cpp:48-50` | `glDeleteTextures` called separately for each onion texture instead of in one call |
| MEDIUM | `src/panels/canvas_panel.cpp:307-312` | Floating selection state fragile on tool re-selection — partially handled |
| LOW | `src/input_handler.cpp:20` | Tool shortcuts (B/E/F…) fire even during active layer rename dialogs |

---

## Summary

| Severity | Count |
|---|---|
| HIGH | 3 |
| MEDIUM | 16 |
| LOW | 16 |
| **Total** | **35** |
