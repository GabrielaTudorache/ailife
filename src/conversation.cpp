#include "conversation.h"

#include <nlohmann/json.hpp>

std::string toneName(Tone tone) {
    switch (tone) {
    case Tone::Warm:
        return "warm";
    case Tone::Neutral:
        return "neutral";
    case Tone::Cold:
        return "cold";
    }
    return "neutral";
}

Tone parseTone(std::string_view name) {
    if (name == "warm") {
        return Tone::Warm;
    }
    if (name == "cold") {
        return Tone::Cold;
    }
    return Tone::Neutral;
}

std::string endReasonName(EndReason reason) {
    switch (reason) {
    case EndReason::Natural:
        return "natural";
    case EndReason::Timeout:
        return "timeout";
    case EndReason::PartnerGone:
        return "partner_gone";
    case EndReason::Limit:
        return "limit";
    }
    return "natural";
}

std::optional<EndReason> parseEndReason(std::string_view name) {
    if (name == "natural") {
        return EndReason::Natural;
    }
    if (name == "timeout") {
        return EndReason::Timeout;
    }
    if (name == "partner_gone") {
        return EndReason::PartnerGone;
    }
    if (name == "limit") {
        return EndReason::Limit;
    }
    return std::nullopt;
}

long long timestampMs(std::chrono::system_clock::time_point tp) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
}

nlohmann::json messageToJson(const Message& message) {
    nlohmann::json j;
    j["type"] = message.is_end ? "end" : "message";
    j["from_pid"] = message.from_pid;
    j["to_pid"] = message.to_pid;
    j["from_name"] = message.from_name;
    j["to_name"] = message.to_name;
    j["content"] = message.content;
    j["tone"] = toneName(message.tone);
    j["sequence"] = message.sequence;
    j["timestamp_ms"] = timestampMs(message.timestamp);
    if (message.end_reason.has_value()) {
        j["end_reason"] = endReasonName(*message.end_reason);
    } else {
        j["end_reason"] = nullptr;
    }
    return j;
}

Message messageFromJson(const nlohmann::json& j) {
    Message m;
    const auto type = j.at("type").get<std::string>();
    m.is_end = (type == "end");
    m.from_pid = j.at("from_pid").get<int>();
    m.to_pid = j.at("to_pid").get<int>();
    m.from_name = j.at("from_name").get<std::string>();
    m.to_name = j.at("to_name").get<std::string>();
    m.content = j.value("content", std::string{});
    m.tone = parseTone(j.value("tone", std::string{"neutral"}));
    m.sequence = j.value("sequence", 0);
    const long long ms = j.at("timestamp_ms").get<long long>();
    m.timestamp = std::chrono::system_clock::time_point{std::chrono::milliseconds{ms}};
    if (j.contains("end_reason") && !j.at("end_reason").is_null()) {
        m.end_reason = parseEndReason(j.at("end_reason").get<std::string>());
    }
    return m;
}
