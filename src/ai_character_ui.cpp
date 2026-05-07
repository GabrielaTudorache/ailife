#include "ai_character_ui.h"

#include "mascot_renderer.h"

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <utility>
#include <vector>

namespace {
constexpr int kJournalPageStep = 10;

ftxui::Color statColor(float value, bool high_is_good = true) {
    const float score = high_is_good ? value : 100.0F - value;
    if (score < 25.0F) {
        return ftxui::Color::Red;
    }
    if (score < 50.0F) {
        return ftxui::Color::Yellow;
    }
    return ftxui::Color::Green;
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

std::string clockText(std::chrono::seconds value) {
    const auto total = std::max<long long>(0, value.count());
    std::ostringstream out;
    out << std::setw(2) << std::setfill('0') << total / 60 << ':' << std::setw(2) << std::setfill('0') << total % 60;
    return out.str();
}

ftxui::Element statBar(const std::string& label, float value, bool high_is_good = true) {
    const float clamped = std::clamp(value, 0.0F, 100.0F);
    return ftxui::vbox({ftxui::hbox({ftxui::text(label), ftxui::filler(),
                                     ftxui::text(std::to_string(static_cast<int>(clamped)) + "/100")}),
                        ftxui::gauge(clamped / 100.0F) | ftxui::color(statColor(clamped, high_is_good))});
}

ftxui::Element mascotBox(const UISnapshot& snapshot) {
    std::vector<ftxui::Element> lines;
    for (const auto& line :
         MascotRenderer::frameFor(pickMascotState(snapshot.stage, snapshot.mood, snapshot.last_action))) {
        lines.push_back(ftxui::text(line));
    }
    return ftxui::vbox(std::move(lines)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 17);
}

constexpr int kTagWidth = 7;

std::string padTag(const std::string& tag) {
    if (tag.size() >= static_cast<std::size_t>(kTagWidth)) {
        return tag;
    }
    return tag + std::string(static_cast<std::size_t>(kTagWidth) - tag.size(), ' ');
}

ftxui::Element headerBox(const UISnapshot& snapshot) {
    return ftxui::hbox({mascotBox(snapshot), ftxui::separator(),
                        ftxui::vbox({ftxui::text(snapshot.name) | ftxui::bold,
                                     ftxui::text("archetype: " + snapshot.archetype) | ftxui::dim,
                                     ftxui::text("stage: " + lifeStageName(snapshot.stage)),
                                     ftxui::text("mood: " + moodLabel(snapshot.mood)) |
                                         ftxui::color(statColor(snapshot.mood))}) |
                            ftxui::flex}) |
           ftxui::borderRounded;
}

ftxui::Element statsBox(const UISnapshot& snapshot) {
    auto body = ftxui::vbox({statBar("hunger", static_cast<float>(snapshot.hunger)),
                             statBar("energy", static_cast<float>(snapshot.energy)), statBar("mood", snapshot.mood),
                             statBar("loneliness", static_cast<float>(snapshot.loneliness), false)});
    return ftxui::window(ftxui::text(" Vitals ") | ftxui::bold, body) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 32);
}

ftxui::Element relationshipsBox() {
    auto body = ftxui::vbox({ftxui::text("the world is quiet.") | ftxui::dim});
    return ftxui::window(ftxui::text(" Relationships ") | ftxui::bold, body) | ftxui::flex;
}

ftxui::Element journalBox(const UISnapshot& snapshot, int scroll_target) {
    const int n = static_cast<int>(snapshot.feed.size());
    const bool following = scroll_target < 0;
    auto title = ftxui::hbox(
        {ftxui::text(" Journal ") | ftxui::bold, following ? ftxui::text("") : ftxui::text("↑ ") | ftxui::dim});

    if (n == 0) {
        return ftxui::window(title, ftxui::text("· · ·") | ftxui::dim | ftxui::center) | ftxui::flex;
    }

    const int focus_idx = following ? n - 1 : std::clamp(scroll_target, 0, n - 1);
    std::vector<ftxui::Element> lines;
    lines.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        const auto& line = snapshot.feed[i];
        auto row = ftxui::hbox({
            ftxui::text(clockText(line.at)) | ftxui::dim,
            ftxui::text(" "),
            ftxui::text(" " + padTag(line.tag) + " ") | ftxui::color(ftxui::Color::Black) | ftxui::bgcolor(line.color) |
                ftxui::bold,
            ftxui::text("  " + line.text),
        });
        if (i == focus_idx) {
            row = row | ftxui::focus;
        }
        lines.push_back(std::move(row));
    }
    return ftxui::window(title, ftxui::vbox(std::move(lines)) | ftxui::yframe) | ftxui::flex;
}

ftxui::Element footerBox(const UISnapshot& snapshot) {
    const auto lifespan = snapshot.lifespan.count() > 0 ? snapshot.lifespan : snapshot.elapsed;
    std::string left = clockText(snapshot.elapsed) + " / " + clockText(lifespan);
    if (snapshot.ended && !snapshot.memories_path.empty()) {
        left += "  memories: " + snapshot.memories_path.string();
    } else if (snapshot.ended && !snapshot.last_words.empty()) {
        left += "  last words: " + snapshot.last_words;
    }
    return ftxui::hbox({ftxui::text(left) | ftxui::dim, ftxui::filler(),
                        ftxui::text("j/k scroll  g/G top/bottom  ") | ftxui::dim,
                        ftxui::text("q quit") | ftxui::bold}) |
           ftxui::borderRounded;
}
} // namespace

AICharacterUI::AICharacterUI(ftxui::ScreenInteractive* screen) : screen_(screen) {}

ftxui::Component AICharacterUI::renderer() {
    auto view = ftxui::Renderer([this] {
        std::scoped_lock lock{snapshot_.mutex};
        return ftxui::vbox({headerBox(snapshot_), ftxui::hbox({statsBox(snapshot_), relationshipsBox()}),
                            journalBox(snapshot_, scroll_target_), footerBox(snapshot_)});
    });
    return ftxui::CatchEvent(view, [this](ftxui::Event event) {
        std::scoped_lock lock{snapshot_.mutex};

        const int n = static_cast<int>(snapshot_.feed.size());
        if (n == 0) {
            return false;
        }

        const int bottom = n - 1;
        const int current = scroll_target_ < 0 ? bottom : std::clamp(scroll_target_, 0, bottom);

        auto set = [&](int target) { scroll_target_ = target >= bottom ? -1 : std::max(0, target); };

        if (event == ftxui::Event::ArrowUp || event == ftxui::Event::Character('k')) {
            set(current - 3);
            return true;
        }
        if (event == ftxui::Event::ArrowDown || event == ftxui::Event::Character('j')) {
            set(current + 3);
            return true;
        }
        if (event == ftxui::Event::PageUp) {
            set(current - kJournalPageStep);
            return true;
        }
        if (event == ftxui::Event::PageDown) {
            set(current + kJournalPageStep);
            return true;
        }
        if (event == ftxui::Event::Home || event == ftxui::Event::Character('g')) {
            scroll_target_ = 0;
            return true;
        }
        if (event == ftxui::Event::End || event == ftxui::Event::Character('G')) {
            scroll_target_ = -1;
            return true;
        }
        if (event.is_mouse()) {
            if (event.mouse().button == ftxui::Mouse::WheelUp) {
                set(current - 3);
                return true;
            }
            if (event.mouse().button == ftxui::Mouse::WheelDown) {
                set(current + 3);
                return true;
            }
        }
        return false;
    });
}

void AICharacterUI::onSimulationStarted(const AICharacter& character, const std::string& archetype) {
    {
        std::scoped_lock lock{snapshot_.mutex};
        snapshot_.name = character.getName();
        snapshot_.archetype = archetype;
        updateStats(character);
        appendLine("ACT", character.getName() + " begins a " + archetype + " life.", ftxui::Color::Green);
    }
    wake();
}

void AICharacterUI::onTick(const AICharacter& character, std::chrono::seconds elapsed, std::chrono::seconds remaining) {
    {
        std::scoped_lock lock{snapshot_.mutex};
        updateStats(character);
        snapshot_.elapsed = elapsed;
        snapshot_.lifespan = elapsed + remaining;
    }
    wake();
}

void AICharacterUI::onThought(const std::string& thoughts) {
    {
        std::scoped_lock lock{snapshot_.mutex};
        appendLine("THINK", thoughts, ftxui::Color::Cyan);
    }
    wake();
}

void AICharacterUI::onDecision(ActionKind kind) {
    {
        std::scoped_lock lock{snapshot_.mutex};
        snapshot_.last_action = kind;
    }
    // no need to wake here because onAction will be called right after
}

void AICharacterUI::onAction(const AICharacter& character, const Action& action) {
    {
        std::scoped_lock lock{snapshot_.mutex};
        updateStats(character);
        snapshot_.last_action = action.kind;
        appendLine("ACT", character.getName() + " chose " + actionName(action.kind), ftxui::Color::Green);
    }
    wake();
}

void AICharacterUI::onJournal(const std::string& narrative) {
    {
        std::scoped_lock lock{snapshot_.mutex};
        appendLine("JOURNAL", narrative, ftxui::Color::Magenta);
    }
    wake();
}

void AICharacterUI::onSpoke(const std::string& narrative) {
    {
        std::scoped_lock lock{snapshot_.mutex};
        appendLine("SAY", narrative, ftxui::Color::Blue);
    }
    wake();
}

void AICharacterUI::onStageChanged(LifeStage previous, LifeStage current) {
    {
        std::scoped_lock lock{snapshot_.mutex};
        snapshot_.stage = current;
        appendLine("STAGE", lifeStageName(previous) + " -> " + lifeStageName(current), ftxui::Color::Yellow);
    }
    wake();
}

void AICharacterUI::onError(const std::string& message) {
    {
        std::scoped_lock lock{snapshot_.mutex};
        appendLine("ERROR", message, ftxui::Color::Red);
    }
    wake();
}

void AICharacterUI::onDeath(const std::string& last_words) {
    {
        std::scoped_lock lock{snapshot_.mutex};
        snapshot_.ended = true;
        snapshot_.stage = LifeStage::Dying;
        snapshot_.last_words = last_words;
        appendLine("DEATH", last_words, ftxui::Color::Red);
    }
    wake();
}

void AICharacterUI::onMemoriesWritten(const std::filesystem::path& path) {
    {
        std::scoped_lock lock{snapshot_.mutex};
        snapshot_.memories_path = path;
        appendLine("DEATH", "wrote " + path.string(), ftxui::Color::Red);
    }
    wake();
}

void AICharacterUI::updateStats(const AICharacter& character) {
    snapshot_.stage = character.getLifeStage();
    snapshot_.hunger = character.getHunger().getValue();
    snapshot_.energy = character.getEnergy().getValue();
    snapshot_.mood = character.getMood().getValue();
    snapshot_.loneliness = character.getLoneliness().getValue();
}

void AICharacterUI::appendLine(std::string tag, std::string text, ftxui::Color color) {
    snapshot_.feed.push_back(JournalLine{std::move(tag), std::move(text), color, snapshot_.elapsed});
}

void AICharacterUI::wake() {
    if (screen_ != nullptr) {
        screen_->PostEvent(ftxui::Event::Custom);
    }
}
