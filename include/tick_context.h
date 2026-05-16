#ifndef AILIFE_TICK_CONTEXT_H
#define AILIFE_TICK_CONTEXT_H

#include "conversation.h"
#include "conversation_manager.h"
#include "enums.h"
#include "presence_registry.h"

#include <deque>
#include <optional>
#include <string>
#include <vector>

struct InboundContext {
    int partner_pid{0};
    std::string partner_name;
    Message message;
    bool is_active_partner{false};
};

struct TickContext {
    bool can_initiate{false};
    bool can_say_goodbye{false};
    std::vector<PresenceSnapshot> visible_peers;
    std::string active_partner_name;
    std::optional<InboundContext> inbound;
    std::deque<Message> active_transcript;
    std::vector<Message> past_with_partner;
    int active_partner_pid{0};
    int active_message_count{0};
    float relationship_bond{0.0F};
    int relationship_exchanges{0};
    RelationshipType relationship_type{RelationshipType::Stranger};
    bool soft_nudge_to_end{false};
    std::optional<std::string> whisper;
    std::optional<std::string> weather;
    int weather_intensity{0};
};

#endif // AILIFE_TICK_CONTEXT_H
