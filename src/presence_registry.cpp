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
constexpr std::size_t kMaxActivityEntries = 50;

PresenceSnapshot snapshotFromJson(const nlohmann::json& j) {
    PresenceSnapshot s;
    s.pid = j.at("pid").get<int>();
    s.id = j.at("id").get<int>();
    s.name = j.at("name").get<std::string>();
    s.archetype = j.at("archetype").get<std::string>();
    if (j.contains("appearance") && j.at("appearance").is_object()) {
        const auto& appearance = j.at("appearance");
        s.appearance.hat = appearance.value("hat", std::string{});
        s.appearance.eyes = appearance.value("eyes", std::string{});
        s.appearance.mouth = appearance.value("mouth", std::string{});
    }

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

    if (j.contains("talking_to_pid")) {
        s.talking_to_pid = j.at("talking_to_pid").get<int>();
    }
    if (j.contains("talking_to_name")) {
        s.talking_to_name = j.at("talking_to_name").get<std::string>();
    }
    if (j.contains("conversation_msg_count")) {
        s.conversation_msg_count = j.at("conversation_msg_count").get<int>();
    }

    if (j.contains("activity") && j.at("activity").is_array()) {
        for (const auto& e : j.at("activity")) {
            ActivityEntry a;
            a.at = std::chrono::seconds{e.value("at", 0LL)};
            a.tag = e.value("tag", std::string{});
            a.text = e.value("text", std::string{});
            s.activity.push_back(std::move(a));
        }
    }

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
    snapshot_.appearance = character.getAppearance();
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
    std::string entry = actionName(action.kind);
    if (!narrative.empty()) {
        entry += " - " + narrative;
    }
    pushActivity("ACT", std::move(entry));
}

void PresenceWriter::onJournal(const std::string& narrative) {
    std::scoped_lock lock{snapshot_mutex_};
    snapshot_.last_narrative = narrative.size() > kMaxNarrativeLen ? narrative.substr(0, kMaxNarrativeLen) : narrative;
    pushActivity("THINK", narrative);
}

void PresenceWriter::onSpoke(const std::string& narrative) {
    std::scoped_lock lock{snapshot_mutex_};
    snapshot_.last_narrative = narrative.size() > kMaxNarrativeLen ? narrative.substr(0, kMaxNarrativeLen) : narrative;
    pushActivity("SAY", narrative);
}

void PresenceWriter::onStageChanged(LifeStage previous, LifeStage current) {
    std::scoped_lock lock{snapshot_mutex_};
    snapshot_.stage = current;
    pushActivity("STAGE", lifeStageName(previous) + " -> " + lifeStageName(current));
}

void PresenceWriter::onDeath(const std::string& last_words) {
    std::scoped_lock lock{snapshot_mutex_};
    snapshot_.stage = LifeStage::Dying;
    const auto& text = last_words;
    snapshot_.last_narrative = text.size() > kMaxNarrativeLen ? text.substr(0, kMaxNarrativeLen) : text;
    pushActivity("DEATH", last_words);
}

void PresenceWriter::onConversationStarted(int partner_pid, const std::string& partner_name) {
    std::scoped_lock lock{snapshot_mutex_};
    snapshot_.talking_to_pid = partner_pid;
    snapshot_.talking_to_name = partner_name;
    snapshot_.conversation_msg_count = 1;
    pushActivity("CHAT", "began talking with " + partner_name);
}

void PresenceWriter::onConversationMessage(int partner_pid, int total_count, const Message& /*message*/,
                                           bool /*outgoing*/) {
    std::scoped_lock lock{snapshot_mutex_};
    if (snapshot_.talking_to_pid == partner_pid) {
        snapshot_.conversation_msg_count = total_count;
    }
}

void PresenceWriter::onConversationEnded(int partner_pid, EndReason reason) {
    std::scoped_lock lock{snapshot_mutex_};
    if (snapshot_.talking_to_pid == partner_pid) {
        pushActivity("CHAT", "talk ended with " + snapshot_.talking_to_name + " (" + endReasonName(reason) + ")");
        snapshot_.talking_to_pid = 0;
        snapshot_.talking_to_name.clear();
        snapshot_.conversation_msg_count = 0;
    }
}

void PresenceWriter::pushActivity(std::string tag, std::string text) {
    if (text.size() > kMaxNarrativeLen) {
        text.resize(kMaxNarrativeLen);
    }
    snapshot_.activity.push_back({snapshot_.elapsed, std::move(tag), std::move(text)});
    if (snapshot_.activity.size() > kMaxActivityEntries) {
        snapshot_.activity.erase(snapshot_.activity.begin(),
                                 snapshot_.activity.begin() +
                                     static_cast<std::ptrdiff_t>(snapshot_.activity.size() - kMaxActivityEntries));
    }
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
        j["appearance"] = {
            {"hat", snap.appearance.hat}, {"eyes", snap.appearance.eyes}, {"mouth", snap.appearance.mouth}};
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
        j["talking_to_pid"] = snap.talking_to_pid;
        j["talking_to_name"] = snap.talking_to_name;
        j["conversation_msg_count"] = snap.conversation_msg_count;

        nlohmann::json acts = nlohmann::json::array();
        for (const auto& a : snap.activity) {
            acts.push_back({{"at", a.at.count()}, {"tag", a.tag}, {"text", a.text}});
        }
        j["activity"] = std::move(acts);

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
