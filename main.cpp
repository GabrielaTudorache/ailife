#include "cli.h"
#include "creator_runner.h"
#include "presence_registry.h"
#include "simulation.h"
#include "ui_runner.h"

#include <exception>
#include <iostream>
#include <utility>

int main(int argc, char* argv[]) {
    try {
        auto config = Cli::parse(argc, argv);
        if (config.creator) {
            return CreatorRunner::run();
        }
        if (!config.headless) {
            return UIRunner::run(std::move(config));
        }
        Simulation simulation{std::move(config)};
        PresenceWriter presence;
        simulation.addObserver(&presence);
        presence.start();
        simulation.run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "fatal: " << error.what() << '\n';
        return 1;
    }
}
