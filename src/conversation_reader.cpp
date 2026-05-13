#include "conversation_reader.h"

#include "conversation_channel.h"

#include <algorithm>

std::vector<Message> ConversationReader::lastMessages(int pid_a, int pid_b, int limit, Logger& logger) const {
    ConversationChannel channel{pid_a, pid_b};
    auto messages = channel.read(0, logger);
    std::sort(messages.begin(), messages.end(),
              [](const Message& a, const Message& b) { return timestampMs(a.timestamp) < timestampMs(b.timestamp); });
    if (limit > 0 && static_cast<int>(messages.size()) > limit) {
        messages.erase(messages.begin(), messages.end() - limit);
    }
    return messages;
}

std::vector<Message> ConversationReader::messagesBefore(int pid_a, int pid_b, long long cutoff_ms, int limit,
                                                        Logger& logger) const {
    ConversationChannel channel{pid_a, pid_b};
    auto messages = channel.read(0, logger);
    messages.erase(std::remove_if(messages.begin(), messages.end(),
                                  [&](const Message& m) { return timestampMs(m.timestamp) >= cutoff_ms; }),
                   messages.end());
    std::sort(messages.begin(), messages.end(),
              [](const Message& a, const Message& b) { return timestampMs(a.timestamp) < timestampMs(b.timestamp); });
    if (limit > 0 && static_cast<int>(messages.size()) > limit) {
        messages.erase(messages.begin(), messages.end() - limit);
    }
    return messages;
}
