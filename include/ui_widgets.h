#ifndef AILIFE_UI_WIDGETS_H
#define AILIFE_UI_WIDGETS_H

#include "enums.h"

#include <ftxui/dom/elements.hpp>

#include <chrono>
#include <string>
#include <string_view>

namespace UIWidgets {
ftxui::Element statBar(const std::string& label, float value, bool high_is_good = true);
ftxui::Element mascotBox(LifeStage stage, float mood, ActionKind last_action);

std::string clockText(std::chrono::seconds value);
std::string moodLabel(float mood);

ftxui::Color tagColor(std::string_view tag);

ftxui::Element journalRow(std::chrono::seconds at, std::string_view tag, ftxui::Color color, const std::string& text);
} // namespace UIWidgets

#endif // AILIFE_UI_WIDGETS_H
