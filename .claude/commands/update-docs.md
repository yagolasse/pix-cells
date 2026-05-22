Use the Agent tool to spawn a subagent with `model: "haiku"` and `subagent_type: "claude"` to carry out all steps below. Pass it the full contents of this prompt as its task. Do not do the work inline.

Run the mandatory post-change verification for pix-cells and update the documentation files so they accurately reflect the current state of the codebase.

## Files to keep in sync

- `/Users/yagolasse/pix-cells/CLAUDE.md` — project overview, stack, build instructions, palette formats table
- `/Users/yagolasse/pix-cells/src/CLAUDE.md` — data model, file roles table, pixel format, dirty-flag pattern, CanvasState method table
- `/Users/yagolasse/pix-cells/src/panels/CLAUDE.md` — panel signatures table, canvas_panel internals, icon font notes, "Adding a new tool" steps
- `/Users/yagolasse/pix-cells/README.md` — user-facing feature list, usage instructions, screenshots/GIFs references

## Process

1. Run `git diff HEAD` to see what changed this session.
2. Read each of the four files above.
3. For each CLAUDE.md file, update only the sections that are stale or incomplete given the diff:
   - New struct fields → update the data model or file roles table
   - New tool index → update `ToolsState` comment and "Adding a new tool" steps
   - New panel or changed signature → update the panel signatures table
   - New method on CanvasState → update the method table in src/CLAUDE.md
   - New file → add a row to the file roles table
   - Removed or renamed anything → remove/update the relevant entry
4. For README.md, update it when the diff introduces a user-facing feature (new tool, new panel, new file format support, new keyboard shortcut, new export/import, etc.):
   - Add the feature to the relevant section (Features, Usage, Supported Formats, etc.)
   - Keep the tone user-facing: describe what the user can do, not implementation details
   - If no user-facing feature was added, do not touch README.md
5. Do not rewrite sections that are still accurate — edit only what is stale.
6. Do not add commentary about what you changed; just make the docs correct.
