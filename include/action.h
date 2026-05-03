#ifndef AILIFE_ACTION_H
#define AILIFE_ACTION_H

#include "enums.h"

#include <string>

struct Action {
    ActionKind kind;
    std::string narrative;
    std::string thoughts;
};

#endif // AILIFE_ACTION_H
