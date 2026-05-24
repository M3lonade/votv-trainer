#include "util/log.hpp"

#include <Windows.h>

#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <string>

namespace votv::log {
namespace {

std::mutex g_mutex;
std::ofstream g_stream;
std::filesystem::path g_path;
bool g_console = false;

std::filesystem::path module_directory()
{
    wchar_t buffer[MAX_PATH]{};
    const auto module = GetModuleHandleW(L"VotVTrainer.dll");
    GetModuleFileNameW(module, buffer, MAX_PATH);

    std::filesystem::path result(buffer);
    if (result.has_parent_path()) {
        return result.parent_path();
    }

    return std::filesystem::current_path();
}

void attach_console()
{
    if (g_console) {
        return;
    }

    if (!AllocConsole()) {
        if (GetLastError() != ERROR_ACCESS_DENIED) {
            return;
        }
    }

    FILE* stream = nullptr;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);
    freopen_s(&stream, "CONIN$", "r", stdin);
    SetConsoleTitleW(L"VotV Trainer");
    g_console = true;
}

} // namespace

void init()
{
    std::scoped_lock lock(g_mutex);
    if (g_stream.is_open()) {
        return;
    }

    attach_console();

    g_path = module_directory() / "VotVTrainer.log";
    g_stream.open(g_path, std::ios::out | std::ios::trunc);
    if (g_stream.is_open()) {
        g_stream << "VotVTrainer log started\n";
        g_stream.flush();
    }

    if (g_console) {
        std::cout << "VotVTrainer console attached\n";
        std::cout << "Log file: " << g_path.string() << '\n';
        std::cout.flush();
    }
}

void shutdown()
{
    std::scoped_lock lock(g_mutex);
    if (g_stream.is_open()) {
        g_stream << "VotVTrainer log stopped\n";
        g_stream.flush();
        g_stream.close();
    }

    if (g_console) {
        std::cout << "VotVTrainer shutting down\n";
        std::cout.flush();
        FreeConsole();
        g_console = false;
    }
}

void write(std::string_view message)
{
    std::scoped_lock lock(g_mutex);

    if (g_console) {
        std::cout << message << '\n';
        std::cout.flush();
    }

    if (g_stream.is_open()) {
        g_stream << message << '\n';
        g_stream.flush();
    }
}

void write_format(const char* format, ...)
{
    char buffer[2048]{};

    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    write(buffer);
}

std::filesystem::path path()
{
    std::scoped_lock lock(g_mutex);
    return g_path;
}

bool console_attached()
{
    std::scoped_lock lock(g_mutex);
    return g_console;
}

} // namespace votv::log
