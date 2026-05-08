#include "presence_registry.h"

#include "paths.h"

#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <process.h>
#define GET_PID() _getpid()
#else
#include <unistd.h>
#define GET_PID() getpid()
#endif

#include <algorithm>
#include <chrono>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>

namespace {
constexpr int kHeartbeatSeconds = 5;
constexpr std::size_t kMaxNarrativeLen = 200;

PresenceSnapshot snapshotFromJson(const nlohmann::json& j) {
    PresenceSnapshot s;
    s.pid = j.at("pid").get<int>();
    s.id = j.at("id").get<int>();
    s.name = j.at("name").get<std::string>();
    s.archetype = j.at("archetype").get<std::string>();

    s.stage = parseLifeStage(j.at("stage").get<std::string>());

    s.hunger = j.at("hunger").get<int>();
    s.energy = j.at("energy").get<int>();
    s.mood = j.at("mood").get<float>();
    s.loneliness = j.at("loneliness").get<int>();
    s.elapsed = std::chrono::seconds{j.at("elapsed_seconds").get<long long>()};
    s.lifespan = std::chrono::seconds{j.at("lifespan_seconds").get<long long>()};

    s.last_action = parseActionKind(j.at("last_action").get<std::string>());

    s.last_narrative = j.at("last_narrative").get<std::string>();

    const long long ms = j.at("last_heartbeat_ms").get<long long>();
    s.last_heartbeat = std::chrono::system_clock::time_point{std::chrono::milliseconds{ms}};

    return s;
}
} // namespace

// PresenceWriter

PresenceWriter::PresenceWriter() {
    const auto dir = Paths::presenceDirectory();
    const std::string base = "ai_" + std::to_string(GET_PID());
    file_path_ = dir / (base + ".json");
    tmp_path_ = dir / (base + ".json.tmp");
    snapshot_.pid = GET_PID();
}

PresenceWriter::~PresenceWriter() {
    stop();
}

void PresenceWriter::start() {
    if (running_.exchange(true)) {
        return;
    }
    thread_ = std::thread{[this] { heartbeatLoop(); }};
}

void PresenceWriter::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    stop_requested_.store(true);
    stop_cv_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
    removeFile();
}

void PresenceWriter::onSimulationStarted(const AICharacter& character, const std::string& archetype) {
    std::scoped_lock lock{snapshot_mutex_};
    snapshot_.id = AICharacter::getCreatedCount();
    snapshot_.name = character.getName();
    snapshot_.archetype = archetype;
    snapshot_.stage = character.getLifeStage();
    snapshot_.hunger = character.getHunger().getValue();
    snapshot_.energy = character.getEnergy().getValue();
    snapshot_.mood = character.getMood().getValue();
    snapshot_.loneliness = character.getLoneliness().getValue();
}

void PresenceWriter::onTick(const AICharacter& character, std::chrono::seconds elapsed,
                            std::chrono::seconds remaining) {
    std::scoped_lock lock{snapshot_mutex_};
    snapshot_.stage = character.getLifeStage();
    snapshot_.hunger = character.getHunger().getValue();
    snapshot_.energy = character.getEnergy().getValue();
    snapshot_.mood = character.getMood().getValue();
    snapshot_.loneliness = character.getLoneliness().getValue();
    snapshot_.elapsed = elapsed;
    snapshot_.lifespan = elapsed + remaining;
}

void PresenceWriter::onAction(const AICharacter& character, const Action& action) {
    std::scoped_lock lock{snapshot_mutex_};
    snapshot_.stage = character.getLifeStage();
    snapshot_.hunger = character.getHunger().getValue();
    snapshot_.energy = character.getEnergy().getValue();
    snapshot_.mood = character.getMood().getValue();
    snapshot_.loneliness = character.getLoneliness().getValue();
    snapshot_.last_action = action.kind;
    const auto& narrative = action.narrative;
    snapshot_.last_narrative = narrative.size() > kMaxNarrativeLen ? narrative.substr(0, kMaxNarrativeLen) : narrative;
}

void PresenceWriter::onJournal(const std::string& narrative) {
    std::scoped_lock lock{snapshot_mutex_};
    snapshot_.last_narrative = narrative.size() > kMaxNarrativeLen ? narrative.substr(0, kMaxNarrativeLen) : narrative;
}

void PresenceWriter::onSpoke(const std::string& narrative) {
    std::scoped_lock lock{snapshot_mutex_};
    snapshot_.last_narrative = narrative.size() > kMaxNarrativeLen ? narrative.substr(0, kMaxNarrativeLen) : narrative;
}

void PresenceWriter::onStageChanged(LifeStage /*previous*/, LifeStage current) {
    std::scoped_lock lock{snapshot_mutex_};
    snapshot_.stage = current;
}

void PresenceWriter::onDeath(const std::string& last_words) {
    std::scoped_lock lock{snapshot_mutex_};
    snapshot_.stage = LifeStage::Dying;
    const auto& text = last_words;
    snapshot_.last_narrative = text.size() > kMaxNarrativeLen ? text.substr(0, kMaxNarrativeLen) : text;
}

void PresenceWriter::heartbeatLoop() {
    writeOnce();
    while (!stop_requested_.load()) {
        std::unique_lock lock{stop_mutex_};
        stop_cv_.wait_for(lock, std::chrono::seconds{kHeartbeatSeconds}, [&] { return stop_requested_.load(); });
        if (!stop_requested_.load()) {
            writeOnce();
        }
    }
}

void PresenceWriter::writeOnce() {
    try {
        PresenceSnapshot snap;
        {
            std::scoped_lock lock{snapshot_mutex_};
            snap = snapshot_;
        }

        const long long ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();

        nlohmann::json j;
        j["pid"] = snap.pid;
        j["id"] = snap.id;
        j["name"] = snap.name;
        j["archetype"] = snap.archetype;
        j["stage"] = lifeStageName(snap.stage);
        j["hunger"] = snap.hunger;
        j["energy"] = snap.energy;
        j["mood"] = snap.mood;
        j["loneliness"] = snap.loneliness;
        j["elapsed_seconds"] = snap.elapsed.count();
        j["lifespan_seconds"] = snap.lifespan.count();
        j["last_action"] = actionName(snap.last_action);
        j["last_narrative"] = snap.last_narrative;
        j["last_heartbeat_ms"] = ms;

        {
            std::ofstream out{tmp_path_};
            out << j.dump();
        }
        std::filesystem::rename(tmp_path_, file_path_);
    } catch (const std::exception& ex) {
        logger_.event("ERROR", std::string{"presence write failed: "} + ex.what());
    }
}

void PresenceWriter::removeFile() {
    std::error_code ec;
    std::filesystem::remove(file_path_, ec);
}

// PresenceReader

PresenceReader::PresenceReader(std::chrono::seconds fresh_threshold, std::chrono::hours cleanup_threshold)
    : fresh_threshold_{fresh_threshold}, cleanup_threshold_{cleanup_threshold} {}

std::vector<PresenceSnapshot> PresenceReader::scan(Logger& logger) const {
    std::vector<PresenceSnapshot> result;
    const auto dir = Paths::presenceDirectory();
    const auto now = std::chrono::system_clock::now();

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) {
            break;
        }
        const auto& path = entry.path();
        if (path.extension() != ".json" || path.filename().string().substr(0, 3) != "ai_") {
            continue;
        }
        try {
            std::ifstream in{path};
            if (!in) {
                continue;
            }
            const auto j = nlohmann::json::parse(in);
            auto snap = snapshotFromJson(j);
            const auto age = std::chrono::duration_cast<std::chrono::seconds>(now - snap.last_heartbeat);

            if (age > std::chrono::duration_cast<std::chrono::seconds>(cleanup_threshold_)) {
                std::error_code rm_ec;
                std::filesystem::remove(path, rm_ec);
                continue;
            }
            if (age > fresh_threshold_) {
                continue;
            }
            result.push_back(std::move(snap));
        } catch (const nlohmann::json::exception& ex) {
            logger.event("WARN", std::string{"presence parse failed ("} + path.filename().string() + "): " + ex.what());
        } catch (const std::filesystem::filesystem_error& ex) {
            logger.event("WARN", std::string{"presence fs error ("} + path.filename().string() + "): " + ex.what());
        }
    }

    std::sort(result.begin(), result.end(), [](const PresenceSnapshot& a, const PresenceSnapshot& b) {
        if (a.id != b.id) {
            return a.id < b.id;
        }
        return a.name < b.name;
    });

    return result;
}
