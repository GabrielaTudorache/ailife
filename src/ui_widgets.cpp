#include "ui_widgets.h"

#include "mascot_renderer.h"

#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace {
constexpr std::size_t kTagWidth = 7;

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

std::string padTag(std::string_view tag) {
    if (tag.size() >= kTagWidth) {
        return std::string{tag};
    }
    std::string out{tag};
    out.append(kTagWidth - tag.size(), ' ');
    return out;
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

std::string clockText(std::chrono::seconds value) {
    const auto total = std::max<long long>(0, value.count());
    std::ostringstream out;
    out << std::setw(2) << std::setfill('0') << total / 60 << ':' << std::setw(2) << std::setfill('0') << total % 60;
    return out.str();
}

std::string moodLabel(float mood) {
    if (mood >= 65.0F) {
        return "bright";
    }
    if (mood <= 35.0F) {
        return "low";
    }
    return "steady";
}

ftxui::Color tagColor(std::string_view tag) {
    if (tag == "ACT") {
        return ftxui::Color::Green;
    }
    if (tag == "THINK") {
        return ftxui::Color::Cyan;
    }
    if (tag == "SAY" || tag == "SAID") {
        return ftxui::Color::Blue;
    }
    if (tag == "HEARD") {
        return ftxui::Color::Magenta;
    }
    if (tag == "CHAT") {
        return ftxui::Color::Magenta;
    }
    if (tag == "JOURNAL") {
        return ftxui::Color::Magenta;
    }
    if (tag == "STAGE") {
        return ftxui::Color::Yellow;
    }
    if (tag == "DEATH") {
        return ftxui::Color::Red;
    }
    if (tag == "ERROR") {
        return ftxui::Color::Red;
    }
    return ftxui::Color::GrayLight;
}

ftxui::Element journalRow(std::chrono::seconds at, std::string_view tag, ftxui::Color color, const std::string& text) {
    return ftxui::hbox({
        ftxui::text(clockText(at)) | ftxui::dim,
        ftxui::text(" "),
        ftxui::text(" " + padTag(tag) + " ") | ftxui::color(ftxui::Color::Black) | ftxui::bgcolor(color) | ftxui::bold,
        ftxui::text("  "),
        ftxui::paragraph(text) | ftxui::flex,
    });
}

} // namespace UIWidgets
