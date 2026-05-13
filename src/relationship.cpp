#include "relationship.h"

#include <algorithm>

namespace {
constexpr float kDecayStep = 0.0005F;
constexpr float kFriendThreshold = 0.20F;
constexpr float kRivalThreshold = -0.20F;
constexpr int kFriendMessages = 4;
constexpr int kRivalMessages = 2;
} // namespace

float Relationship::applyToneDelta(float bond, Tone tone, bool receiver_side) {
    float delta = 0.0F;
    switch (tone) {
    case Tone::Warm:
        delta = receiver_side ? 0.06F : 0.04F;
        break;
    case Tone::Neutral:
        delta = 0.01F;
        break;
    case Tone::Cold:
        delta = receiver_side ? -0.05F : -0.03F;
        break;
    }
    return std::clamp(bond + delta, -1.0F, 1.0F);
}

RelationshipType Relationship::classify(float bond, int messages_exchanged) {
    if (bond >= kFriendThreshold && messages_exchanged >= kFriendMessages) {
        return RelationshipType::Friend;
    }
    if (bond <= kRivalThreshold && messages_exchanged >= kRivalMessages) {
        return RelationshipType::Rival;
    }
    return RelationshipType::Stranger;
}

float Relationship::decay(float bond) {
    if (bond > 0.0F) {
        return std::max(0.0F, bond - kDecayStep);
    }
    if (bond < 0.0F) {
        return std::min(0.0F, bond + kDecayStep);
    }
    return bond;
}
