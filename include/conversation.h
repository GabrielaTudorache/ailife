#ifndef AILIFE_CONVERSATION_H
#define AILIFE_CONVERSATION_H

#include <nlohmann/json_fwd.hpp>

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

enum class Tone { Warm, Neutral, Cold };
enum class EndReason { Natural, Timeout, PartnerGone, Limit };

struct Message {
    bool is_end{false};
    int from_pid{0};
    int to_pid{0};
    std::string from_name;
    std::string to_name;
    std::string content;
    Tone tone{Tone::Neutral};
    int sequence{0};
    std::chrono::system_clock::time_point timestamp{};
    std::optional<EndReason> end_reason;
};

std::string toneName(Tone tone);
Tone parseTone(std::string_view name);
std::string endReasonName(EndReason reason);
std::optional<EndReason> parseEndReason(std::string_view name);

nlohmann::json messageToJson(const Message& message);
Message messageFromJson(const nlohmann::json& j);

long long timestampMs(std::chrono::system_clock::time_point tp);

#endif // AILIFE_CONVERSATION_H
