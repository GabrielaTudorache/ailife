#include "cli.h"
#include "simulation.h"

#include <exception>
#include <iostream>
#include <utility>

int main(int argc, char* argv[]) {
    try {
        auto config = Cli::parse(argc, argv);
        Simulation simulation{std::move(config)};
        simulation.run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "fatal: " << error.what() << '\n';
        return 1;
    }
}
