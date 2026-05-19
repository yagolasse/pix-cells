# pix-cells

A pixel art editor built in C++.

## Stack
- **Build**: CMake 3.20+, FetchContent
- **Window/Input**: SDL3 (static)
- **UI**: Dear ImGui — docking branch (v1.92+)
- **Renderer**: OpenGL 3.3 core

## Build
```powershell
cmake -B build
cmake --build build
```

## Rules
- **Always build after every code change** to verify it compiles before reporting done.
- Run from `C:\Projects\pix-cells`.
- Executable: `build\Debug\pix-cells.exe`

## Palette File Formats to Implement

Priority order for import/export support:

| Format | Type | Notes |
|--------|------|-------|
| **GPL** (GIMP Palette) | Text | De-facto standard; used by Lospec, Aseprite, most open tools. Stores color names + metadata. |
| **HEX / TXT** | Text | One `#RRGGBB` per line. Trivially easy to parse. Lospec exports it. |
| **PAL / JASC-PAL** | Text | Used by Paint Shop Pro and many retro tools. Lospec exports it. |
| **ASE** (Adobe Swatch Exchange) | Binary | Widely supported across Illustrator, Photoshop, InDesign. Lospec exports it. |
| **ACT** (Adobe Color Table) | Binary | 768-byte blob, exactly 256 RGB colors. Dead simple. |
| **PNG swatch** | Image | 1-pixel-tall image, each pixel is a palette color. Reuse existing image loading. |

> Remove this section once all formats are implemented.

## Architecture
- `src/CLAUDE.md` — data model, file roles, pixel format, dirty-flag pattern
- `src/panels/CLAUDE.md` — panel signatures, canvas_panel internals, how to add a tool
