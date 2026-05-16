#ifndef AILIFE_MASCOT_RENDERER_H
#define AILIFE_MASCOT_RENDERER_H

#include "enums.h"

#include <string>
#include <vector>

enum class MascotState { Happy, Neutral, Sad, Sleeping, Dying };

struct MascotAppearance {
    std::string hat;
    std::string eyes;
    std::string mouth;
};

class MascotRenderer {
  public:
    static std::vector<std::string> frameFor(MascotState state);
    static std::vector<std::string> frameFor(MascotState state, const MascotAppearance& appearance);
};

MascotState pickMascotState(LifeStage stage, float mood, ActionKind last_action);
const std::vector<std::string>& availableHatAssets();
const std::vector<std::string>& availableEyeAssets();
const std::vector<std::string>& availableMouthAssets();
bool isValidHatAsset(const std::string& asset);
bool isValidEyeAsset(const std::string& asset);
bool isValidMouthAsset(const std::string& asset);

#endif // AILIFE_MASCOT_RENDERER_H
