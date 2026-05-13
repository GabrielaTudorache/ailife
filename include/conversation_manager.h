#ifndef AILIFE_CONVERSATION_MANAGER_H
#define AILIFE_CONVERSATION_MANAGER_H

#include "conversation.h"
#include "conversation_channel.h"
#include "logger.h"
#include "presence_registry.h"

#include <chrono>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct InboxUpdate {
    int partner_pid{0};
    std::string partner_name;
    Message message;
    bool is_new_conversation{false};
    bool is_active_partner{false};
};

class ConversationManager {
  public:
    ConversationManager(int my_pid, std::string my_name, std::chrono::system_clock::time_point birth_at,
                        Logger& logger);

    std::vector<InboxUpdate> pollInbox(const std::vector<PresenceSnapshot>& presences);

    bool initiate(int partner_pid, std::string partner_name, const std::string& opener, Tone tone);

    void reply(const std::string& text, Tone tone);

    void closeActive(EndReason reason, const std::string& closing_text);

    std::optional<EndReason> handleTimeouts(const std::vector<PresenceSnapshot>& presences,
                                            std::chrono::seconds timeout = std::chrono::seconds{30});

    bool hasActiveConversation() const;
    std::optional<int> activePartnerPid() const;
    std::optional<std::string> activePartnerName() const;
    int activeMessageCount() const;
    const std::deque<Message>& activeTranscript() const;
    std::chrono::system_clock::time_point lastIncomingAt() const;

  private:
    void clearActive();
    long long birthMs() const;
    int parsePartnerPidFromDir(const std::string& dir_name) const;

    int my_pid_;
    std::string my_name_;
    std::chrono::system_clock::time_point birth_at_;
    Logger* logger_;

    int active_partner_pid_{0};
    std::string active_partner_name_;
    std::chrono::system_clock::time_point active_started_at_{};
    std::chrono::system_clock::time_point last_incoming_at_{};
    int next_sequence_{1};

    std::deque<Message> active_messages_;
    std::unique_ptr<ConversationChannel> active_channel_;

    std::unordered_map<int, long long> last_seen_ms_per_channel_;
};

#endif // AILIFE_CONVERSATION_MANAGER_H
