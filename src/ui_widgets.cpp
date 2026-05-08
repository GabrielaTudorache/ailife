#include "ui_widgets.h"

#include "mascot_renderer.h"

#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace {
ftxui::Color statColor(float value, bool high_is_good) {
    const float score = high_is_good ? value : 100.0F - value;
    if (score < 25.0F) {
        return ftxui::Color::Red;
    }
    if (score < 50.0F) {
        return ftxui::Color::Yellow;
    }
    return ftxui::Color::Green;
}
} // namespace

namespace UIWidgets {

ftxui::Element statBar(const std::string& label, float value, bool high_is_good) {
    const float clamped = std::clamp(value, 0.0F, 100.0F);
    return ftxui::vbox({ftxui::hbox({ftxui::text(label), ftxui::filler(),
                                     ftxui::text(std::to_string(static_cast<int>(clamped)) + "/100")}),
                        ftxui::gauge(clamped / 100.0F) | ftxui::color(statColor(clamped, high_is_good))});
}

ftxui::Element mascotBox(LifeStage stage, float mood, ActionKind last_action) {
    std::vector<ftxui::Element> lines;
    for (const auto& line : MascotRenderer::frameFor(pickMascotState(stage, mood, last_action))) {
        lines.push_back(ftxui::text(line));
    }
    return ftxui::vbox(std::move(lines)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 17);
}

} // namespace UIWidgets
