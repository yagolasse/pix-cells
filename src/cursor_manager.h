#pragma once

namespace cursor_manager {
    void init(const char* icons_dir);
    void set_for_tool(int tool_index, bool mouse_pressed = false);
    void shutdown();
}
