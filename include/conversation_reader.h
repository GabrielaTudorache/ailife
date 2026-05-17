#ifndef AILIFE_CONVERSATION_READER_H
#define AILIFE_CONVERSATION_READER_H

#include "conversation.h"
#include "logger.h"

#include <vector>

class ConversationReader {
  public:
    static std::vector<Message> lastMessages(int pid_a, int pid_b, int limit, const Logger& logger);
    static std::vector<Message> messagesBefore(int pid_a, int pid_b, long long cutoff_ms, int limit,
                                               const Logger& logger);
};

#endif // AILIFE_CONVERSATION_READER_H
