#include "conversation_channel.h"

#include "paths.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <string>
#include <system_error>

namespace {
std::string channelName(int pid_a, int pid_b) {
    const int pid1 = std::min(pid_a, pid_b);
    const int pid2 = std::max(pid_a, pid_b);
    return std::to_string(pid1) + "_" + std::to_string(pid2);
}
} // namespace

std::filesystem::path ConversationChannel::directoryFor(int pid_a, int pid_b) {
    return Paths::conversationsDirectory() / channelName(pid_a, pid_b);
}

ConversationChannel::ConversationChannel(int my_pid, int partner_pid)
    : my_pid_(my_pid), dir_(directoryFor(my_pid, partner_pid)) {}

long long ConversationChannel::append(const Message& message) {
    std::filesystem::create_directories(dir_);

    const long long now_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    long long ts_ms = std::max(now_ms, last_emitted_ms_ + 1);
    last_emitted_ms_ = ts_ms;

    Message stamped = message;
    stamped.timestamp = std::chrono::system_clock::time_point{std::chrono::milliseconds{ts_ms}};

    const std::string base = "msg_" + std::to_string(ts_ms) + "_" + std::to_string(my_pid_);
    const auto tmp_path = dir_ / (base + ".tmp");
    const auto final_path = dir_ / (base + ".json");

    {
        std::ofstream out{tmp_path};
        out << messageToJson(stamped).dump();
    }
    std::filesystem::rename(tmp_path, final_path);
    return ts_ms;
}

std::vector<Message> ConversationChannel::read(long long since_ms, const Logger& logger) const {
    std::vector<Message> messages;
    std::error_code ec;
    if (!std::filesystem::exists(dir_, ec)) {
        return messages;
    }
    for (const auto& entry : std::filesystem::directory_iterator(dir_, ec)) {
        if (ec) {
            break;
        }
        const auto& path = entry.path();
        if (path.extension() != ".json") {
            continue;
        }
        if (path.filename().string().substr(0, 4) != "msg_") {
            continue;
        }
        try {
            std::ifstream in{path};
            if (!in) {
                continue;
            }
            const auto j = nlohmann::json::parse(in);
            Message m = messageFromJson(j);
            const long long ts = timestampMs(m.timestamp);
            if (ts < since_ms) {
                continue;
            }
            messages.push_back(std::move(m));
        } catch (const nlohmann::json::exception& ex) {
            logger.event("WARN",
                         std::string{"conversation parse failed ("} + path.filename().string() + "): " + ex.what());
        } catch (const std::filesystem::filesystem_error& ex) {
            logger.event("WARN", std::string{"conversation fs error ("} + path.filename().string() + "): " + ex.what());
        }
    }
    std::sort(messages.begin(), messages.end(),
              [](const Message& a, const Message& b) { return timestampMs(a.timestamp) < timestampMs(b.timestamp); });
    return messages;
}
