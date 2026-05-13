#include "ai_character_ui.h"

#include "ui_widgets.h"

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <utility>
#include <vector>

namespace {
constexpr int kJournalPageStep = 10;

ftxui::Color moodColor(float mood) {
    if (mood < 25.0F) {
        return ftxui::Color::Red;
    }
    if (mood < 50.0F) {
        return ftxui::Color::Yellow;
    }
    return ftxui::Color::Green;
}

ftxui::Element speechBubble(const SpeechBubble& bubble) {
    if (bubble.text.empty()) {
        return ftxui::text("");
    }
    const auto age = std::chrono::steady_clock::now() - bubble.at;
    if (age > std::chrono::seconds{8}) {
        return ftxui::text("");
    }
    const std::string arrow = bubble.outgoing ? "» " : "« ";
    auto color = bubble.outgoing ? ftxui::Color::Blue : ftxui::Color::Magenta;
    auto elem = ftxui::text(arrow + bubble.text) | ftxui::color(color);
    if (age > std::chrono::seconds{4}) {
        elem = elem | ftxui::dim;
    }
    return elem;
}

ftxui::Element headerBox(const UISnapshot& snapshot) {
    std::vector<ftxui::Element> right;
    right.push_back(ftxui::text(snapshot.name) | ftxui::bold);
    right.push_back(ftxui::text("archetype: " + snapshot.archetype) | ftxui::dim);
    right.push_back(ftxui::text("stage: " + lifeStageName(snapshot.stage)));
    right.push_back(ftxui::text("mood: " + UIWidgets::moodLabel(snapshot.mood)) |
                    ftxui::color(moodColor(snapshot.mood)));
    if (!snapshot.active_partner_name.empty()) {
        right.push_back(ftxui::text("talking with: " + snapshot.active_partner_name) | ftxui::bold);
    }
    auto in_bubble = speechBubble(snapshot.last_incoming);
    auto out_bubble = speechBubble(snapshot.last_outgoing);
    if (!snapshot.last_incoming.text.empty() ||
        std::chrono::steady_clock::now() - snapshot.last_incoming.at < std::chrono::seconds{8}) {
        right.push_back(in_bubble);
    }
    if (!snapshot.last_outgoing.text.empty() ||
        std::chrono::steady_clock::now() - snapshot.last_outgoing.at < std::chrono::seconds{8}) {
        right.push_back(out_bubble);
    }
    return ftxui::hbox({UIWidgets::mascotBox(snapshot.stage, snapshot.mood, snapshot.last_action), ftxui::separator(),
                        ftxui::vbox(std::move(right)) | ftxui::flex}) |
           ftxui::borderRounded;
}

ftxui::Element statsBox(const UISnapshot& snapshot) {
    auto body = ftxui::vbox({UIWidgets::statBar("hunger", static_cast<float>(snapshot.hunger)),
                             UIWidgets::statBar("energy", static_cast<float>(snapshot.energy)),
                             UIWidgets::statBar("mood", snapshot.mood),
                             UIWidgets::statBar("loneliness", static_cast<float>(snapshot.loneliness), false)});
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
        auto row = UIWidgets::journalRow(line.at, line.tag, line.color, line.text);
        if (i == focus_idx) {
            row = row | ftxui::focus;
        }
        lines.push_back(std::move(row));
    }
    return ftxui::window(title, ftxui::vbox(std::move(lines)) | ftxui::yframe) | ftxui::flex;
}

ftxui::Element footerBox(const UISnapshot& snapshot) {
    const auto lifespan = snapshot.lifespan.count() > 0 ? snapshot.lifespan : snapshot.elapsed;
    std::string left = UIWidgets::clockText(snapshot.elapsed) + " / " + UIWidgets::clockText(lifespan);
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

void AICharacterUI::onConversationStarted(int /*partner_pid*/, const std::string& partner_name) {
    {
        std::scoped_lock lock{snapshot_.mutex};
        snapshot_.active_partner_name = partner_name;
        appendLine("CHAT", "began talking with " + partner_name, ftxui::Color::Cyan);
    }
    wake();
}

void AICharacterUI::onConversationMessage(int /*partner_pid*/, int /*total_count*/, const Message& message,
                                          bool outgoing) {
    {
        std::scoped_lock lock{snapshot_.mutex};
        SpeechBubble& slot = outgoing ? snapshot_.last_outgoing : snapshot_.last_incoming;
        slot.text = message.content;
        slot.outgoing = outgoing;
        slot.at = std::chrono::steady_clock::now();
        if (outgoing) {
            appendLine("SAID", message.content, ftxui::Color::Blue);
        } else {
            appendLine("HEARD", message.from_name + ": " + message.content, ftxui::Color::Magenta);
        }
    }
    wake();
}

void AICharacterUI::onConversationEnded(int /*partner_pid*/, EndReason reason) {
    {
        std::scoped_lock lock{snapshot_.mutex};
        appendLine("CHAT", "talk ended (" + endReasonName(reason) + ")", ftxui::Color::Cyan);
        snapshot_.active_partner_name.clear();
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
