#include "creator_runner.h"

#include "creator_ui.h"
#include "logger.h"
#include "presence_registry.h"

#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

int CreatorRunner::run() {
    auto screen = ftxui::ScreenInteractive::Fullscreen();
    CreatorUI ui{&screen};
    Logger logger;
    PresenceReader reader;

    std::atomic<bool> stop_requested{false};
    std::mutex stop_mutex;
    std::condition_variable stop_cv;

    std::thread poll_thread{[&] {
        while (!stop_requested.load()) {
            try {
                auto presences = reader.scan(logger);
                ui.setPresences(std::move(presences), std::chrono::system_clock::now());
            } catch (const std::exception& ex) {
                ui.setError(ex.what());
            }
            std::unique_lock lock{stop_mutex};
            stop_cv.wait_for(lock, std::chrono::seconds{1}, [&] { return stop_requested.load(); });
        }
    }};

    auto root = ui.renderer() | ftxui::CatchEvent([&](const ftxui::Event& event) {
                    if (event == ftxui::Event::Character('q') || event == ftxui::Event::Character('Q')) {
                        stop_requested.store(true);
                        stop_cv.notify_all();
                        if (poll_thread.joinable()) {
                            poll_thread.join();
                        }
                        screen.ExitLoopClosure()();
                        return true;
                    }
                    return false;
                });

    screen.Loop(root);

    stop_requested.store(true);
    stop_cv.notify_all();
    if (poll_thread.joinable()) {
        poll_thread.join();
    }
    return 0;
}
