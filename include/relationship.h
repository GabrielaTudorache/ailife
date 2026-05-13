#ifndef AILIFE_RELATIONSHIP_H
#define AILIFE_RELATIONSHIP_H

#include "conversation.h"
#include "enums.h"

#include <chrono>
#include <string>

struct Relationship {
    RelationshipType type{RelationshipType::Stranger};
    float bond{0.0F};
    int messages_exchanged{0};
    int conversations_count{0};
    std::chrono::system_clock::time_point last_interaction{};
    std::string partner_name;

    static float applyToneDelta(float bond, Tone tone, bool receiver_side);
    static RelationshipType classify(float bond, int messages_exchanged);
    static float decay(float bond);
};

#endif // AILIFE_RELATIONSHIP_H
