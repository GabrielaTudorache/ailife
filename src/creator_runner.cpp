#include "creator_runner.h"

#include "conversation_reader.h"
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

    ConversationReader conversation_reader;

    std::thread poll_thread{[&] {
        int last_selected_pid = 0;
        while (!stop_requested.load()) {
            try {
                auto presences = reader.scan(logger);
                int selected_partner_pid = 0;
                int selected_self_pid = 0;
                const int selected_pid_initial = ui.selectedPid();
                for (const auto& p : presences) {
                    if (p.pid == selected_pid_initial) {
                        selected_self_pid = p.pid;
                        selected_partner_pid = p.talking_to_pid;
                        break;
                    }
                }
                // wipe conversation if selected changes
                if (selected_pid_initial != last_selected_pid) {
                    ui.setSelectedTranscript({});
                    last_selected_pid = selected_pid_initial;
                }
                ui.setPresences(std::move(presences), std::chrono::system_clock::now());
                if (selected_self_pid != 0 && selected_partner_pid != 0) {
                    auto messages =
                        conversation_reader.lastMessages(selected_self_pid, selected_partner_pid, 5, logger);
                    ui.setSelectedTranscript(std::move(messages));
                }
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
