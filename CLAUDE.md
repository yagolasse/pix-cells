# pix-cells

A pixel art editor built in C++.

## Stack
- **Build**: CMake 3.20+, FetchContent
- **Window/Input**: SDL3 (static)
- **UI**: Dear ImGui — docking branch (v1.92+)
- **Renderer**: OpenGL 3.3 core
- **Fonts**: Ubuntu Regular (`fonts/Ubuntu-Regular.ttf`) — UI font loaded at 5 scales: 11px (75%), 15px (100%), 19px (125%), 23px (150%), 30px (200%); scale persisted to `SDL_GetPrefPath/settings.ini`.
- **Icons**: SVG files in `icons/`, rasterized at runtime via lunasvg (FetchContent, v3.2.1). Accessed via `icon_manager::get("name")` → `ImTextureID`. Naming convention: `name.svg` / `name_filled.svg` / `name_off.svg` / `name_open.svg`.

## Build
```bash
cmake -B build
cmake --build build
ctest --test-dir build -C Debug --output-on-failure
```

## Lint
```bash
cmake --build build --target lint
```
(Requires `clang-tidy` in PATH; if not found, the lint target is unavailable.)

## Rules
- **Always build and run tests after every code change** — both must pass before reporting done.
- **Always update the docs (CLAUDE.md files) after code changes**

## Refactoring Status

All 35 audit findings are complete (see `REFACTOR_PLAN.md` for details). Notable recent changes:
- **Blend optimization**: Moved per-pixel float division to a compile-time LUT (`k_u8f`) in `blend.h`, reducing hot-path overhead. Blending math corrected for proper Porter-Duff behavior.
- **Selection extraction**: Moved `lift_selection()` and `commit_floating()` to new `selection.cpp` file, out of `canvas_state.cpp`.
- **Logging coverage**: Added ~40 `Log()` calls throughout the UI for debugging and user feedback (layer edits, tool settings, file I/O, palette operations, etc.).
- **Input safety**: Tool shortcuts now blocked during active layer rename dialogs via `layers_panel_is_renaming()`.
- **Paste as selection**: Pasting now creates an active selection, allowing users to move/scale before committing.

## Palette File Formats to Implement

Priority order for import/export support:

| Format | Type | Notes |
|--------|------|-------|
| **PAL / JASC-PAL** | Text | Used by Paint Shop Pro and many retro tools. Lospec exports it. |
| **ASE** (Adobe Swatch Exchange) | Binary | Widely supported across Illustrator, Photoshop, InDesign. Lospec exports it. |
| **ACT** (Adobe Color Table) | Binary | 768-byte blob, exactly 256 RGB colors. Dead simple. |
| **PNG swatch** | Image | 1-pixel-tall image, each pixel is a palette color. Reuse existing image loading. |

> Remove this section once all formats are implemented.

## Architecture
- `src/CLAUDE.md` — data model, file roles, pixel format, dirty-flag pattern
- `src/panels/CLAUDE.md` — panel signatures, canvas_panel internals, how to add a tool
