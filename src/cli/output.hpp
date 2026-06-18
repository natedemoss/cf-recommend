// Terminal output helpers: optional ANSI colour and small formatting utilities.
#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#include <io.h>
#define CFR_ISATTY(fd) _isatty(fd)
#define CFR_FILENO(f) _fileno(f)
#else
#include <unistd.h>
#define CFR_ISATTY(fd) isatty(fd)
#define CFR_FILENO(f) fileno(f)
#endif

namespace cfr::out {

// Colour is on for interactive terminals, but disabled when NO_COLOR is set
// (https://no-color.org) or when stdout is redirected to a file or pipe — so
// `cf analyze > out.txt` and `cf next | grep` stay free of escape codes.
inline bool colorEnabled() {
    static const bool enabled = [] {
        if (std::getenv("NO_COLOR") != nullptr) return false;
        return CFR_ISATTY(CFR_FILENO(stdout)) != 0;
    }();
    return enabled;
}

inline std::string paint(const std::string& s, const char* code) {
    if (!colorEnabled()) return s;
    return std::string("\033[") + code + "m" + s + "\033[0m";
}

inline std::string bold(const std::string& s) { return paint(s, "1"); }
inline std::string dim(const std::string& s) { return paint(s, "2"); }
inline std::string red(const std::string& s) { return paint(s, "31"); }
inline std::string green(const std::string& s) { return paint(s, "32"); }
inline std::string yellow(const std::string& s) { return paint(s, "33"); }
inline std::string cyan(const std::string& s) { return paint(s, "36"); }

// Colours a success-rate percentage: red (weak) -> yellow -> green (strong).
inline std::string ratePaint(const std::string& s, double rate) {
    if (rate < 0.40) return red(s);
    if (rate < 0.70) return yellow(s);
    return green(s);
}

}  // namespace cfr::out
