#include "cli.h"

#include <chrono>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
bool isArchetype(std::string_view value) {
    return value == "curious" || value == "cautious" || value == "warm" || value == "gloomy";
}
} // namespace

namespace Cli {
std::string usage() {
    return "usage: ailife --creator\n"
           "       ailife --spawn --name <name> --archetype <curious|cautious|warm|gloomy>\n"
           "                      [--duration <minutes>] [--mock-llm] [--headless]";
}

Config parse(int argc, char* argv[]) {
    Config config;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};
        if (arg == "--creator") {
            config.creator = true;
        } else if (arg == "--spawn") {
            config.spawn = true;
        } else if (arg == "--name") {
            if (++i >= argc) {
                throw std::invalid_argument("--name requires a value\n" + usage());
            }
            config.name = argv[i];
        } else if (arg == "--archetype") {
            if (++i >= argc) {
                throw std::invalid_argument("--archetype requires a value\n" + usage());
            }
            config.archetype = argv[i];
        } else if (arg == "--duration") {
            if (++i >= argc) {
                throw std::invalid_argument("--duration requires a value\n" + usage());
            }
            const double minutes = std::stod(argv[i]);
            if (minutes <= 0.0) {
                throw std::invalid_argument("--duration must be positive\n" + usage());
            }
            config.duration = std::chrono::seconds{static_cast<int>(std::ceil(minutes * 60.0))};
        } else if (arg == "--mock-llm") {
            config.mock_llm = true;
        } else if (arg == "--headless") {
            config.headless = true;
        } else {
            throw std::invalid_argument("unknown argument: " + std::string{arg} + '\n' + usage());
        }
    }

    if (config.creator && config.spawn) {
        throw std::invalid_argument("--creator and --spawn are mutually exclusive\n" + usage());
    }
    if (!config.creator && !config.spawn) {
        throw std::invalid_argument("must pass --creator or --spawn\n" + usage());
    }
    if (config.spawn) {
        if (config.name.empty()) {
            throw std::invalid_argument("--name is required with --spawn\n" + usage());
        }
        if (!isArchetype(config.archetype)) {
            throw std::invalid_argument("unknown archetype: " + config.archetype + '\n' + usage());
        }
    }
    return config;
}
} // namespace Cli
