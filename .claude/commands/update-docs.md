Review the changes made this session and update the CLAUDE.md documentation files so they accurately reflect the current state of the codebase.

## Files to keep in sync

- `/Users/yagolasse/pix-cells/CLAUDE.md` — project overview, stack, build instructions, palette formats table
- `/Users/yagolasse/pix-cells/src/CLAUDE.md` — data model, file roles table, pixel format, dirty-flag pattern, CanvasState method table
- `/Users/yagolasse/pix-cells/src/panels/CLAUDE.md` — panel signatures table, canvas_panel internals, icon font notes, "Adding a new tool" steps

## Process

1. Run `git diff HEAD` to see what changed this session.
2. Read each of the three CLAUDE.md files above.
3. For each file, update only the sections that are stale or incomplete given the diff:
   - New struct fields → update the data model or file roles table
   - New tool index → update `ToolsState` comment and "Adding a new tool" steps
   - New panel or changed signature → update the panel signatures table
   - New method on CanvasState → update the method table in src/CLAUDE.md
   - New file → add a row to the file roles table
   - Removed or renamed anything → remove/update the relevant entry
4. Do not rewrite sections that are still accurate — edit only what is stale.
5. Do not add commentary about what you changed; just make the docs correct.
