#ifndef AILIFE_CONVERSATION_READER_H
#define AILIFE_CONVERSATION_READER_H

#include "conversation.h"
#include "logger.h"

#include <vector>

class ConversationReader {
  public:
    std::vector<Message> lastMessages(int pid_a, int pid_b, int limit, Logger& logger) const;
    std::vector<Message> messagesBefore(int pid_a, int pid_b, long long cutoff_ms, int limit, Logger& logger) const;
};

#endif // AILIFE_CONVERSATION_READER_H
