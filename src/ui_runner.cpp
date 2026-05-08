#include "ui_runner.h"

#include "ai_character_ui.h"
#include "presence_registry.h"
#include "simulation.h"

#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <exception>
#include <thread>
#include <utility>

int UIRunner::run(Cli::Config config) {
    auto screen = ftxui::ScreenInteractive::Fullscreen();
    Simulation simulation{std::move(config)};
    AICharacterUI ui{&screen};
    PresenceWriter presence;
    simulation.addObserver(&ui);
    simulation.addObserver(&presence);
    presence.start();

    std::thread sim_thread{[&] {
        try {
            simulation.run();
        } catch (const std::exception& error) {
            ui.onError(error.what());
        }
        screen.PostEvent(ftxui::Event::Custom);
    }};

    auto root = ui.renderer() | ftxui::CatchEvent([&](const ftxui::Event& event) {
                    if (event == ftxui::Event::Character('q') || event == ftxui::Event::Character('Q')) {
                        simulation.requestStop();
                        if (sim_thread.joinable()) {
                            sim_thread.join();
                        }
                        screen.ExitLoopClosure()();
                        return true;
                    }
                    return false;
                });

    screen.Loop(root);
    if (sim_thread.joinable()) {
        simulation.requestStop();
        sim_thread.join();
    }
    return 0;
}
