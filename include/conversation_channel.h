#ifndef AILIFE_CONVERSATION_CHANNEL_H
#define AILIFE_CONVERSATION_CHANNEL_H

#include "conversation.h"
#include "logger.h"

#include <filesystem>
#include <vector>

class ConversationChannel {
  public:
    ConversationChannel(int my_pid, int partner_pid);

    long long append(const Message& message);

    std::vector<Message> read(long long since_ms, const Logger& logger) const;

    static std::filesystem::path directoryFor(int pid_a, int pid_b);

  private:
    int my_pid_;
    std::filesystem::path dir_;
    long long last_emitted_ms_{0};
};

#endif // AILIFE_CONVERSATION_CHANNEL_H
