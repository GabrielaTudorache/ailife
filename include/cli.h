#ifndef AILIFE_CLI_H
#define AILIFE_CLI_H

#include <chrono>
#include <string>

namespace Cli {
struct Config {
    bool spawn{false};
    std::string name;
    std::string archetype{"curious"};
    std::chrono::seconds duration{std::chrono::minutes{8}};
    bool mock_llm{false};
    bool headless{false};
};

Config parse(int argc, char** argv);
std::string usage();
} // namespace Cli

#endif // AILIFE_CLI_H
