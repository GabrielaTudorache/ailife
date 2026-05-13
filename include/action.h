#ifndef AILIFE_ACTION_H
#define AILIFE_ACTION_H

#include "conversation.h"
#include "enums.h"

#include <string>

struct Action {
    ActionKind kind;
    std::string narrative;
    Tone tone = Tone::Neutral;
};

#endif // AILIFE_ACTION_H
