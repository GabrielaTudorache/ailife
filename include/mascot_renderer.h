#ifndef AILIFE_MASCOT_RENDERER_H
#define AILIFE_MASCOT_RENDERER_H

#include "enums.h"

#include <string>
#include <vector>

enum class MascotState { Happy, Neutral, Sad, Sleeping, Dying };

class MascotRenderer {
  public:
    static std::vector<std::string> frameFor(MascotState state);
};

MascotState pickMascotState(LifeStage stage, float mood, ActionKind last_action);

#endif // AILIFE_MASCOT_RENDERER_H
