#pragma once

#include <filesystem>
#include <mutex>
#include <string_view>

namespace votv::log {

void init();
void shutdown();
void write(std::string_view message);
void write_format(const char* format, ...);
std::filesystem::path path();
bool console_attached();

} // namespace votv::log
