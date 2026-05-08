#ifndef AILIFE_UI_WIDGETS_H
#define AILIFE_UI_WIDGETS_H

#include "enums.h"

#include <ftxui/dom/elements.hpp>

#include <string>

namespace UIWidgets {
ftxui::Element statBar(const std::string& label, float value, bool high_is_good = true);
ftxui::Element mascotBox(LifeStage stage, float mood, ActionKind last_action);
} // namespace UIWidgets

#endif // AILIFE_UI_WIDGETS_H
