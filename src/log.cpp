#include "log.h"
#include <cstdio>
#include <cstdarg>
#ifdef _MSC_VER
#pragma warning(disable : 4996) // fopen is fine here
#endif

static std::deque<std::string> s_entries;
static FILE* s_file              = nullptr;
static constexpr int MAX_ENTRIES = 500;

void Log(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (!s_file)
        s_file = fopen("pix-cells.log", "w");
    if (s_file) {
        fprintf(s_file, "%s\n", buf);
        fflush(s_file);
    }

    s_entries.emplace_back(buf);
    if ((int)s_entries.size() > MAX_ENTRIES)
        s_entries.pop_front();
}

const std::deque<std::string>& LogEntries() {
    return s_entries;
}

void LogClear() {
    s_entries.clear();
}
