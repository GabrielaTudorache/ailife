#include "logger.h"

#include <iostream>

#if defined(_WIN32)
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

namespace {
std::string_view colorFor(std::string_view tag) {
    if (tag == "RESET") {
        return "\033[0m";
    }
    if (tag == "TICK") {
        return "\033[2m";
    }
    if (tag == "DECIDE") {
        return "\033[36m";
    }
    if (tag == "ACT") {
        return "\033[33m";
    }
    if (tag == "JOURNAL") {
        return "\033[32m";
    }
    if (tag == "STAGE") {
        return "\033[35m";
    }
    if (tag == "LLM") {
        return "\033[34m";
    }
    if (tag == "DEATH") {
        return "\033[31m";
    }
    if (tag == "ERROR") {
        return "\033[91m";
    }
    return "";
}
} // namespace

Logger::Logger() : use_color_(isatty(fileno(stdout)) != 0) {}

void Logger::event(std::string_view tag, const std::string& text) const {
    if (use_color_) {
        std::cout << colorFor(tag) << '[' << tag << ']' << colorFor("RESET") << ' ' << text << '\n';
    } else {
        std::cout << '[' << tag << "] " << text << '\n';
    }
}
