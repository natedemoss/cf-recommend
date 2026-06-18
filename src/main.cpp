#include <exception>
#include <iostream>

#include "cli/commands.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// Make the Windows console behave like a modern terminal: UTF-8 output (so the
// em-dashes and bullets render correctly) and ANSI escape support (so colours
// aren't printed as literal "\033[..." sequences).
static void prepareWindowsConsole() {
    SetConsoleOutputCP(CP_UTF8);
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode)) {
        SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
}
#endif

int main(int argc, char** argv) {
#ifdef _WIN32
    prepareWindowsConsole();
#endif
    try {
        return cfr::dispatch(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
}
