#pragma once
#include <deque>
#include <string>

void Log(const char* fmt, ...);
const std::deque<std::string>& LogEntries();
void LogClear();
