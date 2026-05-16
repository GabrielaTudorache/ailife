#include "creator_runner.h"

#include "command.h"
#include "conversation_reader.h"
#include "creator_ui.h"
#include "logger.h"
#include "paths.h"
#include "presence_registry.h"
#include "spawn_process.h"

#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
std::string randomName(const std::vector<PresenceSnapshot>& presences) {
    static const std::vector<std::string> names{
        "Mara", "Iris", "Niko",  "Sorin", "Ada",   "Luca",   "Rina",   "Tavi",  "Noa",    "Elian",
        "Mira", "Vera", "Cato",  "Anca",  "Toma",  "Radu",   "Ioana",  "Daria", "Milo",   "Nora",
        "Ema",  "Zara", "Felix", "Alma",  "Lina",  "Rhea",   "Theo",   "Kira",  "Vlad",   "Oana",
        "Ilie", "Sana", "Petra", "Dinu",  "Rox",   "Amos",   "Silviu", "Lia",   "Victor", "Mina",
        "Cora", "Arin", "Selma", "Nadia", "Matei", "Bianca", "Eli",    "Dora",  "Calin",  "Irina"};
    static std::mt19937 rng{std::random_device{}()};
    std::vector<std::string> available;
    for (const auto& name : names) {
        const bool used =
            std::any_of(presences.begin(), presences.end(), [&](const PresenceSnapshot& p) { return p.name == name; });
        if (!used) {
            available.push_back(name);
        }
    }
    if (available.empty()) {
        return "AI" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    }
    std::uniform_int_distribution<std::size_t> dist{0, available.size() - 1};
    return available[dist(rng)];
}

std::string uniqueName(std::string desired, const std::vector<PresenceSnapshot>& presences) {
    if (desired.empty()) {
        return randomName(presences);
    }
    auto used = [&](const std::string& candidate) {
        return std::any_of(presences.begin(), presences.end(),
                           [&](const PresenceSnapshot& p) { return p.name == candidate; });
    };
    if (!used(desired)) {
        return desired;
    }
    for (int suffix = 2; suffix < 100; ++suffix) {
        const std::string candidate = desired + "_" + std::to_string(suffix);
        if (!used(candidate)) {
            return candidate;
        }
    }
    return desired + "_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
}

bool isPendingEventFile(const std::filesystem::path& path) {
    const std::string name = path.filename().string();
    return name.rfind("pending_", 0) == 0 && path.extension() == ".json";
}

void sweepOldBroadcastEvents() {
    const auto now = std::filesystem::file_time_type::clock::now();
    const auto events = Paths::eventsDirectory();
    const auto applied = Paths::appliedEventsDirectory();
    for (const auto& entry : std::filesystem::directory_iterator(events)) {
        if (!entry.is_regular_file() || !isPendingEventFile(entry.path())) {
            continue;
        }
        const auto age = now - entry.last_write_time();
        if (age < std::chrono::seconds{60}) {
            continue;
        }
        auto target = applied / entry.path().filename();
        if (std::filesystem::exists(target)) {
            target += ".old";
        }
        std::filesystem::rename(entry.path(), target);
    }
}
} // namespace

int CreatorRunner::run(const Cli::Config& config) {
    auto screen = ftxui::ScreenInteractive::Fullscreen();
    Logger logger;
    PresenceReader reader;
    std::mutex spawned_mutex;
    std::vector<long> spawned_pids;

    CreatorActions actions;
    actions.spawnConfigured = [&](const SpawnOptions& options) {
        auto final_options = options;
        final_options.name = uniqueName(final_options.name, reader.scan(logger));
        const long pid = SpawnProcess::spawnHeadlessAI(final_options, config.mock_llm);
        std::scoped_lock lock{spawned_mutex};
        spawned_pids.push_back(pid);
    };
    actions.weather = [](const std::string& condition, int intensity) {
        CommandEmitter::emit(WeatherCommand{condition, intensity});
    };
    actions.gift = [](int pid, const std::string& item) { CommandEmitter::emit(GiftCommand{pid, item}); };
    actions.whisper = [](int pid, const std::string& message) { CommandEmitter::emit(WhisperCommand{pid, message}); };
    actions.moodNudge = [](int pid, float delta) { CommandEmitter::emit(MoodNudgeCommand{pid, delta}); };
    actions.kill = [](int pid) { CommandEmitter::emit(KillCommand{pid}); };
    CreatorUI ui{&screen, std::move(actions)};

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
                sweepOldBroadcastEvents();
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

    for (long pid : spawned_pids) {
        SpawnProcess::terminate(pid);
    }
    return 0;
}
