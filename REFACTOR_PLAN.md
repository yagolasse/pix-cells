# pix-cells Refactor & Improvement Plan

Audit date: 2026-05-28. 35 findings across 7 categories.
Last updated: 2026-05-28 — checkerboard texture, symmetry dedup, floating commit, SDL_GetPrefPath verify, renderToBitmap guard complete.

---

## Status legend
- ✅ Done
- 🔲 Open

---

## Completed (commit bfd0e61)

| Item | Notes |
|------|-------|
| ✅ Timeline thumbnail caching | `frame_revisions` counter + `s_seen_revisions` in timeline_panel |
| ✅ `pixc_io.cpp` fread safety | All fread returns checked; dimension validation (1–4096) |
| ✅ `raster.cpp` integer overflow | `size_t` cast before `w*h`; `nn_scale` zero-dim guard |
| ✅ Split `canvas_panel.cpp` | canvas_overlay.cpp + canvas_tools.cpp + canvas_panel_internal.h |
| ✅ Round-trip I/O error tests | 4 new test_pixc cases (truncated, zero/oversized dims, partial pixels) |
| ✅ `k_hmoves` bounds check | `assert(active_handle >= 0 && < 8)` in canvas_tools.cpp |
| ✅ `DocRenderState` to header | Now in `canvas_panel_internal.h` |

---

## Next priority order

1. ~~**Quick wins**~~ ✅
2. ~~**Test coverage**~~ ✅
3. ~~**Onion skin dirty flag**~~ ✅
4. ~~**Marching ants pre-generation**~~ ✅
5. ~~**Centralize PendingIO**~~ ✅

---

## 1. Memory Leaks / Resource Safety

| Severity | File | Finding | Status |
|---|---|---|---|
| ~~HIGH~~ | ~~`src/pixc_io.cpp:140-141`~~ | ~~fread return values unchecked~~ | ✅ |
| ~~MEDIUM~~ | ~~`src/ui_scale.cpp:44`~~ | ~~`SDL_GetPrefPath` result not freed in all error paths~~ | ✅ |
| ~~MEDIUM~~ | ~~`src/icon_manager.cpp:60`~~ | ~~`renderToBitmap` width/height never validated before texture upload~~ | ✅ |

---

## 2. Code Smells / Refactors

| Severity | File | Finding | Status |
|---|---|---|---|
| ~~MEDIUM~~ | ~~`src/input_handler.cpp:88-140`~~ | ~~Copy and Cut share identical selection-iteration loops — extract `fill_clipboard()` helper~~ | ✅ |
| ~~MEDIUM~~ | ~~`src/panels/canvas_tools.cpp`~~ | ~~Symmetry mirroring logic duplicated between `draw_px` preview and `sym_pt`/`sym_seg` commit paths~~ | ✅ |
| ~~MEDIUM~~ | ~~`src/panels/canvas_overlay.cpp`~~ | ~~Checkerboard drawn as O(rows×cols) rects per frame — should use a tiled texture instead~~ | ✅ |
| ~~LOW~~ | ~~`src/panels/canvas_tools.cpp`~~ | ~~`update_selection_from_float()` pattern duplicated twice~~ | ✅ |
| LOW | `src/panels/canvas_tools.cpp` | Color picker tool (tool 10) is a hardcoded branch instead of following the tool pattern | 🔲 |
| ~~LOW~~ | ~~`src/ui_scale.cpp:22`~~ | ~~Magic `99.f` sentinel should be `std::numeric_limits<float>::max()`~~ | ✅ |

---

## 3. Architecture / Folder Restructuring

| Severity | File | Finding | Status |
|---|---|---|---|
| ~~MEDIUM~~ | ~~`canvas_panel.cpp` 756 lines~~ | ~~Split into overlay/tools/coordinator~~ | ✅ |
| ~~MEDIUM~~ | ~~`canvas_panel.cpp:29-37`~~ | ~~`DocRenderState` private — move to header~~ | ✅ |
| ~~MEDIUM~~ | ~~`src/panels/menu_bar.cpp:23` + `palette_panel.cpp:23`~~ | ~~`static PendingIO` scattered across panels — centralize into `file_io_context.h`~~ | ✅ |
| LOW | `src/canvas_state.cpp:140-162` | `lift_selection()`/`commit_floating()` only used in canvas_panel — move to `selection.cpp` | 🔲 |

---

## 4. Test Coverage Gaps

| Severity | File | Finding | Status |
|---|---|---|---|
| ~~MEDIUM~~ | ~~`src/raster.cpp`~~ | ~~`color_select()` and `nn_scale()` edge cases untested (disconnected regions, thin lines, zero-dim)~~ | ✅ |
| ~~MEDIUM~~ | ~~`src/pixc_io.cpp`~~ | ~~No error-path tests for file format~~ | ✅ |
| ~~MEDIUM~~ | ~~`src/blend.h:27-35`~~ | ~~Blend mode unit tests missing for edge cases (fully transparent src, saturating Add)~~ | ✅ |
| ~~MEDIUM~~ | ~~`src/raster.cpp:95-101`~~ | ~~Complex selection masks (disconnected regions, thin lines) untested~~ | ✅ |

---

## 5. Performance

| Severity | File | Finding | Status |
|---|---|---|---|
| ~~HIGH~~ | ~~`src/panels/timeline_panel.cpp:128-131`~~ | ~~Per-frame composite for every thumbnail~~ | ✅ |
| ~~MEDIUM~~ | ~~`src/panels/canvas_panel.cpp:292-305`~~ | ~~Onion skin recomposited every frame even when nothing changed — add dirty flag~~ | ✅ |
| ~~MEDIUM~~ | ~~`src/panels/canvas_overlay.cpp`~~ | ~~Marching ants outline regenerated every frame — pre-generate outline geometry~~ | ✅ |
| MEDIUM | `src/blend.h:7-35` | Per-pixel float division in compositing hot path — consider SIMD or LUTs | 🔲 |
| LOW | `src/input_handler.cpp:101,121` | `clipboard.resize()` called even when size unchanged | 🔲 |
| LOW | `src/panels/layers_panel.cpp:52` | Minor: string concat with `+` instead of reserve/`std::to_string` pre-alloc | 🔲 |

---

## 6. Safety & Correctness

| Severity | File | Finding | Status |
|---|---|---|---|
| ~~HIGH~~ | ~~`src/raster.cpp:63`~~ | ~~`w * h` overflow before `size_t` cast~~ | ✅ |
| ~~MEDIUM~~ | ~~`canvas_tools.cpp`~~ | ~~`k_hmoves[active_handle]` no bounds check~~ | ✅ |
| ~~MEDIUM~~ | ~~`src/raster.cpp:130-133`~~ | ~~`nn_scale()` no zero-dim guard~~ | ✅ |
| ~~MEDIUM~~ | ~~`src/pixc_io.cpp:113-134`~~ | ~~Canvas dimensions never validated before allocation~~ | ✅ |
| LOW | `src/panels/canvas_overlay.cpp` | Signed `sel.x0/y0` used in float arithmetic without explicit cast | 🔲 |
| LOW | `src/app_state.cpp:21` | `hex_to_imvec4()` silently forces alpha=1.0f — alpha handling undocumented | 🔲 |
| LOW | `src/cursor_manager.cpp:38` | `SDL_CreateColorCursor` failure not logged — silent invisible cursor | 🔲 |

---

## 7. Other / Correctness

| Severity | File | Finding | Status |
|---|---|---|---|
| ~~MEDIUM~~ | ~~`src/panels/canvas_panel.cpp:48-50`~~ | ~~`glDeleteTextures` called separately for each onion texture — batch into one call~~ | ✅ |
| ~~MEDIUM~~ | ~~`src/panels/canvas_tools.cpp`~~ | ~~Floating selection state fragile on tool re-selection — partially handled~~ | ✅ |
| LOW | `src/input_handler.cpp:20` | Tool shortcuts (B/E/F…) fire even during active layer rename dialogs | 🔲 |

---

## Summary

| Severity | Original | Done | Remaining |
|---|---|---|---|
| HIGH | 3 | 3 | 0 |
| MEDIUM | 20 | 19 | 1 |
| LOW | 10 | 2 | 8 |
| **Total** | **33** | **24** | **9** |
