#include "cli.h"

#include "mascot_renderer.h"

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
    return "usage: ailife --creator [--mock-llm]\n"
           "       ailife --spawn --name <name> --archetype <curious|cautious|warm|gloomy>\n"
           "                      [--duration <minutes>] [--mock-llm] [--headless]\n"
           "                      [--hat <asset>] [--eyes <asset>] [--mouth <asset>]\n"
           "                      [--openness <0-100>] [--conscientiousness <0-100>]\n"
           "                      [--extraversion <0-100>] [--agreeableness <0-100>]\n"
           "                      [--neuroticism <0-100>] [--quirk <text>]";
}

int parseTrait(const char* value, const std::string& flag) {
    const int parsed = std::stoi(value);
    if (parsed < 0 || parsed > 100) {
        throw std::invalid_argument(flag + " must be between 0 and 100\n" + usage());
    }
    return parsed;
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
        } else if (arg == "--hat") {
            if (++i >= argc) {
                throw std::invalid_argument("--hat requires a value\n" + usage());
            }
            config.appearance.hat = argv[i];
        } else if (arg == "--eyes") {
            if (++i >= argc) {
                throw std::invalid_argument("--eyes requires a value\n" + usage());
            }
            config.appearance.eyes = argv[i];
        } else if (arg == "--mouth") {
            if (++i >= argc) {
                throw std::invalid_argument("--mouth requires a value\n" + usage());
            }
            config.appearance.mouth = argv[i];
        } else if (arg == "--openness") {
            if (++i >= argc)
                throw std::invalid_argument("--openness requires a value\n" + usage());
            config.personality.openness = parseTrait(argv[i], "--openness");
            config.custom_personality = true;
        } else if (arg == "--conscientiousness") {
            if (++i >= argc)
                throw std::invalid_argument("--conscientiousness requires a value\n" + usage());
            config.personality.conscientiousness = parseTrait(argv[i], "--conscientiousness");
            config.custom_personality = true;
        } else if (arg == "--extraversion") {
            if (++i >= argc)
                throw std::invalid_argument("--extraversion requires a value\n" + usage());
            config.personality.extraversion = parseTrait(argv[i], "--extraversion");
            config.custom_personality = true;
        } else if (arg == "--agreeableness") {
            if (++i >= argc)
                throw std::invalid_argument("--agreeableness requires a value\n" + usage());
            config.personality.agreeableness = parseTrait(argv[i], "--agreeableness");
            config.custom_personality = true;
        } else if (arg == "--neuroticism") {
            if (++i >= argc)
                throw std::invalid_argument("--neuroticism requires a value\n" + usage());
            config.personality.neuroticism = parseTrait(argv[i], "--neuroticism");
            config.custom_personality = true;
        } else if (arg == "--quirk") {
            if (++i >= argc)
                throw std::invalid_argument("--quirk requires a value\n" + usage());
            config.personality.quirk = argv[i];
            config.custom_personality = true;
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
    if (!isValidHatAsset(config.appearance.hat)) {
        throw std::invalid_argument("unknown hat asset: " + config.appearance.hat + '\n' + usage());
    }
    if (!isValidEyeAsset(config.appearance.eyes)) {
        throw std::invalid_argument("unknown eyes asset: " + config.appearance.eyes + '\n' + usage());
    }
    if (!isValidMouthAsset(config.appearance.mouth)) {
        throw std::invalid_argument("unknown mouth asset: " + config.appearance.mouth + '\n' + usage());
    }
    return config;
}
} // namespace Cli
