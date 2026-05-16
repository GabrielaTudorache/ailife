#ifndef AILIFE_UI_SNAPSHOT_H
#define AILIFE_UI_SNAPSHOT_H

#include "enums.h"
#include "mascot_renderer.h"

#include <ftxui/screen/color.hpp>

#include <chrono>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>

struct JournalLine {
    std::string tag;
    std::string text;
    ftxui::Color color;
    std::chrono::seconds at{0};
};

struct SpeechBubble {
    std::string text;
    bool outgoing{false};
    std::chrono::steady_clock::time_point at{};
};

struct UISnapshot {
    std::mutex mutex;
    std::string name;
    std::string archetype;
    LifeStage stage{LifeStage::Young};
    int hunger{0};
    int energy{0};
    int loneliness{0};
    float mood{0.0F};
    std::chrono::seconds elapsed{0};
    std::chrono::seconds lifespan{0};
    ActionKind last_action{ActionKind::WriteJournal};
    MascotAppearance appearance;
    std::deque<JournalLine> feed;
    bool ended{false};
    std::string last_words;
    std::filesystem::path memories_path;
    SpeechBubble last_outgoing;
    SpeechBubble last_incoming;
    std::string active_partner_name;
};

#endif // AILIFE_UI_SNAPSHOT_H
