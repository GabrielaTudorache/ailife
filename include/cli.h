#ifndef AILIFE_CLI_H
#define AILIFE_CLI_H

#include <chrono>
#include <string>

#include "mascot_renderer.h"
#include "personality.h"

namespace Cli {
struct Config {
    bool spawn{false};
    bool creator{false};
    std::string name;
    std::string archetype{"curious"};
    std::chrono::seconds duration{std::chrono::minutes{8}};
    bool mock_llm{false};
    bool headless{false};
    MascotAppearance appearance;
    bool custom_personality{false};
    Personality personality;
};

Config parse(int argc, char** argv);
std::string usage();
} // namespace Cli

#endif // AILIFE_CLI_H
