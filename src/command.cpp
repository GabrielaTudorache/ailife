#include "command.h"

#include "paths.h"
#include "simulation.h"
#include "simulation_exception.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {
constexpr int kMinIntensity = 1;
constexpr int kMaxIntensity = 3;
std::atomic<unsigned long long> sequence{0};

long long nowMs() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

int clampIntensity(int intensity) {
    return std::clamp(intensity, kMinIntensity, kMaxIntensity);
}

std::string requireString(const nlohmann::json& json, const char* key) {
    if (!json.contains(key) || !json.at(key).is_string()) {
        throw CommandParseException{std::string{"command missing string field: "} + key};
    }
    return json.at(key).get<std::string>();
}

int targetFrom(const nlohmann::json& json) {
    if (!json.contains("target_pid") || json.at("target_pid").is_null()) {
        return 0;
    }
    if (!json.at("target_pid").is_number_integer()) {
        throw CommandParseException{"target_pid must be an integer"};
    }
    return json.at("target_pid").get<int>();
}

const nlohmann::json& payloadFrom(const nlohmann::json& json) {
    if (!json.contains("payload") || !json.at("payload").is_object()) {
        throw CommandParseException{"command missing object field: payload"};
    }
    return json.at("payload");
}
} // namespace

WeatherCommand::WeatherCommand(std::string condition, int intensity)
    : Command{0}, condition_{std::move(condition)}, intensity_{clampIntensity(intensity)} {}

void WeatherCommand::apply(Simulation& simulation) const {
    simulation.onWeather(condition_, intensity_);
}

std::string WeatherCommand::type() const {
    return "weather";
}

nlohmann::json WeatherCommand::toJson() const {
    return {{"condition", condition_}, {"intensity", intensity_}};
}

std::string WeatherCommand::description() const {
    return "weather: " + condition_ + " intensity " + std::to_string(intensity_);
}

GiftCommand::GiftCommand(int target_pid, std::string item, std::string note)
    : Command{target_pid}, item_{std::move(item)}, note_{std::move(note)} {}

void GiftCommand::apply(Simulation& simulation) const {
    simulation.onGift(item_, note_);
}

std::string GiftCommand::type() const {
    return "gift";
}

nlohmann::json GiftCommand::toJson() const {
    return {{"item", item_}, {"note", note_}};
}

std::string GiftCommand::description() const {
    return "gift: " + item_;
}

WhisperCommand::WhisperCommand(int target_pid, std::string message)
    : Command{target_pid}, message_{std::move(message)} {
    if (target_pid <= 0) {
        throw CommandParseException{"whisper command requires a target_pid"};
    }
}

void WhisperCommand::apply(Simulation& simulation) const {
    simulation.onWhisper(message_);
}

std::string WhisperCommand::type() const {
    return "whisper";
}

nlohmann::json WhisperCommand::toJson() const {
    return {{"message", message_}};
}

std::string WhisperCommand::description() const {
    return "whisper";
}

MoodNudgeCommand::MoodNudgeCommand(int target_pid, float delta)
    : Command{target_pid}, delta_{std::clamp(delta, -50.0F, 50.0F)} {}

void MoodNudgeCommand::apply(Simulation& simulation) const {
    simulation.onMoodNudge(delta_);
}

std::string MoodNudgeCommand::type() const {
    return "mood_nudge";
}

nlohmann::json MoodNudgeCommand::toJson() const {
    return {{"delta", delta_}};
}

std::string MoodNudgeCommand::description() const {
    return "mood nudge: " + std::to_string(delta_);
}

KillCommand::KillCommand(int target_pid) : Command{target_pid} {
    if (target_pid <= 0) {
        throw CommandParseException{"kill command requires a target_pid"};
    }
}

void KillCommand::apply(Simulation& simulation) const {
    simulation.onForceKill();
}

std::string KillCommand::type() const {
    return "kill";
}

nlohmann::json KillCommand::toJson() const {
    return nlohmann::json::object();
}

std::string KillCommand::description() const {
    return "force kill";
}

namespace CommandIo {
std::unique_ptr<Command> fromJson(const nlohmann::json& json) {
    const std::string type = requireString(json, "type");
    const int target_pid = targetFrom(json);
    const auto& payload = payloadFrom(json);

    if (type == "weather") {
        return std::make_unique<WeatherCommand>(requireString(payload, "condition"), payload.value("intensity", 1));
    }
    if (type == "gift") {
        return std::make_unique<GiftCommand>(target_pid, requireString(payload, "item"), payload.value("note", ""));
    }
    if (type == "whisper") {
        return std::make_unique<WhisperCommand>(target_pid, requireString(payload, "message"));
    }
    if (type == "mood_nudge") {
        return std::make_unique<MoodNudgeCommand>(target_pid, payload.value("delta", 0.0F));
    }
    if (type == "kill") {
        return std::make_unique<KillCommand>(target_pid);
    }
    throw CommandParseException{"unknown command type: " + type};
}

nlohmann::json envelope(const Command& command) {
    const auto ms = nowMs();
    const auto seq = sequence.fetch_add(1);
    std::ostringstream id;
    id << ms << '_' << seq;
    return {{"id", id.str()},
            {"type", command.type()},
            {"target_pid", command.targetPid()},
            {"created_at_ms", ms},
            {"payload", command.toJson()}};
}
} // namespace CommandIo

namespace CommandEmitter {
void emit(const Command& command) {
    const auto json = CommandIo::envelope(command);
    const std::string id = json.at("id").get<std::string>();
    const auto events = Paths::eventsDirectory();
    const auto tmp = events / ("pending_" + id + ".tmp");
    const auto final = events / ("pending_" + id + ".json");

    {
        std::ofstream out{tmp, std::ios::binary};
        if (!out) {
            throw IPCException{"could not write command file: " + tmp.string()};
        }
        out << json.dump(2);
    }
    std::filesystem::rename(tmp, final);
}
} // namespace CommandEmitter
