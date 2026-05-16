#include "creator_ui.h"

#include "paths.h"
#include "ui_widgets.h"

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

std::string moodDot(float mood) {
    if (mood >= 65.0F) {
        return "●";
    }
    if (mood <= 35.0F) {
        return "○";
    }
    return "◎";
}

std::string formatTime(std::chrono::system_clock::time_point tp) {
    const std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    std::ostringstream out;
    out << std::setw(2) << std::setfill('0') << tm_buf.tm_hour << ':' << std::setw(2) << std::setfill('0')
        << tm_buf.tm_min << ':' << std::setw(2) << std::setfill('0') << tm_buf.tm_sec;
    return out.str();
}

std::string assetLabel(const std::string& asset) {
    return asset.empty() ? "random" : asset;
}

ftxui::Element listRow(const PresenceSnapshot& snap, bool selected, bool is_partner) {
    std::string row_text = moodDot(snap.mood) + " " + snap.name + " (" + snap.archetype + ")  " +
                           lifeStageName(snap.stage) + "  mood=" + UIWidgets::moodLabel(snap.mood) +
                           "  age=" + UIWidgets::clockText(snap.elapsed);
    if (snap.talking_to_pid != 0) {
        row_text += "  <-> " + snap.talking_to_name + " (" + std::to_string(snap.conversation_msg_count) + ")";
    }
    auto elem = ftxui::text(row_text);
    if (selected) {
        elem = elem | ftxui::inverted;
    } else if (is_partner) {
        elem = elem | ftxui::color(ftxui::Color::Yellow) | ftxui::bold;
    }
    return elem;
}

ftxui::Color toneColor(Tone tone) {
    switch (tone) {
    case Tone::Warm:
        return ftxui::Color::GreenLight;
    case Tone::Cold:
        return ftxui::Color::CyanLight;
    case Tone::Neutral:
    default:
        return ftxui::Color::GrayLight;
    }
}

std::string derivePartnerName(const PresenceSnapshot& snap, const std::vector<Message>& messages) {
    if (!snap.talking_to_name.empty()) {
        return snap.talking_to_name;
    }
    for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
        if (it->from_pid != snap.pid && !it->from_name.empty()) {
            return it->from_name;
        }
        if (it->to_pid != snap.pid && !it->to_name.empty()) {
            return it->to_name;
        }
    }
    return "(unknown)";
}

ftxui::Element transcriptPanel(const PresenceSnapshot& snap, const std::vector<Message>& messages) {
    const bool active = snap.talking_to_pid != 0;
    const std::string partner = derivePartnerName(snap, messages);

    std::vector<ftxui::Element> lines;
    if (messages.empty()) {
        lines.push_back(ftxui::text("(no messages yet)") | ftxui::dim);
    } else {
        const int last_idx = static_cast<int>(messages.size()) - 1;
        for (int i = 0; i <= last_idx; ++i) {
            const auto& m = messages[i];
            const bool self = m.from_pid == snap.pid;
            const std::string speaker = self ? snap.name : m.from_name;
            const auto speaker_color = self ? ftxui::Color::Blue : ftxui::Color::Magenta;

            auto header = ftxui::hbox({
                ftxui::text(formatTime(m.timestamp)) | ftxui::dim,
                ftxui::text("  "),
                ftxui::text(speaker) | ftxui::color(speaker_color) | ftxui::bold,
                ftxui::text("  ·  ") | ftxui::dim,
                ftxui::text(toneName(m.tone)) | ftxui::color(toneColor(m.tone)) | ftxui::dim,
            });

            ftxui::Element body;
            if (m.is_end) {
                std::string reason = m.end_reason ? endReasonName(*m.end_reason) : "ended";
                std::string text = "[end: " + reason + "]";
                if (!m.content.empty()) {
                    text += " " + m.content;
                }
                body = ftxui::paragraph(text) | ftxui::color(ftxui::Color::Yellow);
            } else {
                body = ftxui::paragraph(m.content);
            }
            if (i == last_idx) {
                body = body | ftxui::bold;
            }

            auto block = ftxui::vbox({
                header,
                ftxui::hbox({ftxui::text("  "), body | ftxui::flex}),
            });
            if (i == last_idx) {
                block = block | ftxui::focus;
            }
            lines.push_back(std::move(block));
        }
    }

    const std::string prefix = active ? "Active conversation" : "Last conversation";
    const std::string title = " " + prefix + " with " + partner + "  (" + std::to_string(messages.size()) + " msgs) ";
    auto title_element = ftxui::text(title) | ftxui::bold;
    if (!active) {
        title_element = title_element | ftxui::dim;
    }
    return ftxui::window(title_element, ftxui::vbox(std::move(lines)) | ftxui::yframe);
}

ftxui::Element activityPanel(const PresenceSnapshot& snap) {
    std::vector<ftxui::Element> rows;
    if (snap.activity.empty()) {
        rows.push_back(ftxui::text("(no activity yet)") | ftxui::dim);
    } else {
        const int last = static_cast<int>(snap.activity.size()) - 1;
        for (int i = 0; i <= last; ++i) {
            const auto& e = snap.activity[i];
            auto row = UIWidgets::journalRow(e.at, e.tag, UIWidgets::tagColor(e.tag), e.text);
            if (i == last) {
                row = row | ftxui::focus;
            }
            rows.push_back(std::move(row));
        }
    }
    return ftxui::window(ftxui::text(" Activity (" + std::to_string(snap.activity.size()) + ") ") | ftxui::bold,
                         ftxui::vbox(std::move(rows)) | ftxui::yframe);
}

constexpr int kActivityRowsWithTranscript = 10;

ftxui::Element detailPanel(const PresenceSnapshot& snap, bool transcript_visible) {
    const auto age =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - snap.last_heartbeat);

    auto mascot = UIWidgets::mascotBox(snap.stage, snap.mood, snap.last_action, snap.appearance);

    std::string header = snap.name + " - " + snap.archetype + "  [" + lifeStageName(snap.stage) + "]";

    auto stats = ftxui::vbox({UIWidgets::statBar("hunger", static_cast<float>(snap.hunger)),
                              UIWidgets::statBar("energy", static_cast<float>(snap.energy)),
                              UIWidgets::statBar("mood", snap.mood),
                              UIWidgets::statBar("loneliness", static_cast<float>(snap.loneliness), false)});

    auto top = ftxui::hbox({mascot, ftxui::separator(),
                            ftxui::vbox({ftxui::text(header) | ftxui::bold, ftxui::separator(), stats}) | ftxui::flex});

    auto footer = ftxui::text("heartbeat " + std::to_string(age.count()) + "s ago") | ftxui::dim;

    auto activity = activityPanel(snap);
    if (transcript_visible) {
        activity = activity | ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, kActivityRowsWithTranscript);
    } else {
        activity = activity | ftxui::flex;
    }

    return ftxui::vbox({std::move(top), std::move(activity), std::move(footer)}) | ftxui::flex;
}

ftxui::Element detailWithTranscript(const PresenceSnapshot& snap, const std::vector<Message>& transcript) {
    const bool transcript_visible = snap.talking_to_pid != 0 || !transcript.empty();
    auto detail = detailPanel(snap, transcript_visible);
    if (!transcript_visible) {
        return detail;
    }
    return ftxui::vbox({detail, transcriptPanel(snap, transcript) | ftxui::flex}) | ftxui::flex;
}

constexpr int kFieldName = 0;
constexpr int kFieldArchetype = 1;
constexpr int kFieldLifespan = 2;
constexpr int kFieldHat = 3;
constexpr int kFieldEyes = 4;
constexpr int kFieldMouth = 5;
constexpr int kFieldOpenness = 6;
constexpr int kFieldConscientiousness = 7;
constexpr int kFieldExtraversion = 8;
constexpr int kFieldAgreeableness = 9;
constexpr int kFieldNeuroticism = 10;
constexpr int kFieldQuirk = 11;
constexpr int kFieldSpawnButton = 12;
constexpr int kFieldCount = 13;

void clampTrait(int& v, int delta) {
    v = std::clamp(v + delta, 0, 100);
}

ftxui::Element labelledRow(int field_index, int selected, const std::string& label, ftxui::Element value) {
    const bool active = field_index == selected;
    auto marker = ftxui::text(active ? "▶ " : "  ");
    auto label_el = ftxui::text(label) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 20);
    if (active) {
        label_el = label_el | ftxui::bold | ftxui::color(ftxui::Color::Yellow);
    }
    return ftxui::hbox({marker, label_el, value | ftxui::flex});
}

ftxui::Element cycleValue(const std::string& value, bool selected) {
    auto v = ftxui::text("◀ " + value + " ▶");
    if (selected) {
        v = v | ftxui::color(ftxui::Color::Yellow);
    }
    return v;
}

ftxui::Element textValue(const std::string& value, const std::string& placeholder, bool selected) {
    const bool empty = value.empty();
    auto t = ftxui::text(empty ? placeholder : value);
    if (empty) {
        t = t | ftxui::dim;
    }
    if (selected) {
        t = t | ftxui::color(ftxui::Color::Yellow);
    }
    return t;
}

ftxui::Element numberValue(int value, const std::string& suffix, bool selected) {
    auto t = ftxui::text("◀ " + std::to_string(value) + " " + suffix + " ▶");
    if (selected) {
        t = t | ftxui::color(ftxui::Color::Yellow);
    }
    return t;
}

ftxui::Element traitBar(int value, bool selected) {
    const int filled = std::clamp(value / 10, 0, 10);
    std::string fill;
    for (int i = 0; i < 10; ++i) {
        fill += (i < filled ? "█" : "░");
    }
    auto bar = ftxui::text(fill);
    bar = bar | ftxui::color(selected ? ftxui::Color::Yellow : ftxui::Color::GrayLight);
    return ftxui::hbox({bar, ftxui::text("  " + std::to_string(value))});
}

ftxui::Element spawnStudio(const CreatorSnapshot& s) {
    const auto& hats = availableHatAssets();
    const auto& eyes = availableEyeAssets();
    const auto& mouths = availableMouthAssets();
    const MascotAppearance preview{hats[s.spawn_hat_index], eyes[s.spawn_eyes_index], mouths[s.spawn_mouth_index]};
    const int sel = s.spawn_field_index;
    const auto& p = s.spawn_personality;

    auto section = [](const std::string& title) {
        return ftxui::text(" " + title + " ") | ftxui::bold | ftxui::color(ftxui::Color::Cyan);
    };

    auto identity = ftxui::vbox({
        section("Identity"),
        labelledRow(kFieldName, sel, "Name", textValue(s.spawn_name, "(random unique)", sel == kFieldName)),
        labelledRow(kFieldArchetype, sel, "Archetype", cycleValue(s.spawn_archetype, sel == kFieldArchetype)),
        labelledRow(kFieldLifespan, sel, "Lifespan",
                    numberValue(static_cast<int>(s.spawn_lifespan_minutes), "min", sel == kFieldLifespan)),
    });

    auto appearance = ftxui::vbox({
        section("Appearance"),
        labelledRow(kFieldHat, sel, "Hat", cycleValue(assetLabel(hats[s.spawn_hat_index]), sel == kFieldHat)),
        labelledRow(kFieldEyes, sel, "Eyes", cycleValue(assetLabel(eyes[s.spawn_eyes_index]), sel == kFieldEyes)),
        labelledRow(kFieldMouth, sel, "Mouth", cycleValue(assetLabel(mouths[s.spawn_mouth_index]), sel == kFieldMouth)),
    });

    auto personality = ftxui::vbox({
        section("Personality"),
        labelledRow(kFieldOpenness, sel, "Openness", traitBar(p.openness, sel == kFieldOpenness)),
        labelledRow(kFieldConscientiousness, sel, "Conscientiousness",
                    traitBar(p.conscientiousness, sel == kFieldConscientiousness)),
        labelledRow(kFieldExtraversion, sel, "Extraversion", traitBar(p.extraversion, sel == kFieldExtraversion)),
        labelledRow(kFieldAgreeableness, sel, "Agreeableness", traitBar(p.agreeableness, sel == kFieldAgreeableness)),
        labelledRow(kFieldNeuroticism, sel, "Neuroticism", traitBar(p.neuroticism, sel == kFieldNeuroticism)),
        labelledRow(kFieldQuirk, sel, "Quirk", textValue(p.quirk, "(none)", sel == kFieldQuirk)),
    });

    auto spawn_text = ftxui::text(" ✦ Spawn this being ✦ ");
    if (sel == kFieldSpawnButton) {
        spawn_text = spawn_text | ftxui::bold | ftxui::inverted;
    }
    auto spawn_button = spawn_text | ftxui::border;

    auto right = ftxui::vbox({
        identity,
        ftxui::separatorEmpty(),
        appearance,
        ftxui::separatorEmpty(),
        personality,
        ftxui::separatorEmpty(),
        ftxui::hbox({ftxui::filler(), spawn_button, ftxui::filler()}),
    });

    auto left = ftxui::vbox({
        section("Preview"),
        UIWidgets::mascotBox(LifeStage::Young, 70.0F, ActionKind::WriteJournal, preview),
        ftxui::text(s.spawn_archetype) | ftxui::center | ftxui::dim,
    });

    auto body = ftxui::hbox({left, ftxui::separator(), right | ftxui::flex});
    auto help =
        ftxui::text("↑/↓ field   ←/→ adjust   Enter edit/spawn   s spawn   r randomize   Esc back") | ftxui::dim;

    auto window =
        ftxui::window(ftxui::text(" Create new being ") | ftxui::bold, ftxui::vbox({body, ftxui::separator(), help})) |
        ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, 80);
    return ftxui::clear_under(window);
}

struct InterveneAction {
    std::string label;
    bool needs_target;
};

const std::vector<InterveneAction>& interveneActions() {
    static const std::vector<InterveneAction> items = {
        {"Sunny day (broadcast)", false},
        {"Breezy (broadcast)", false},
        {"Clear night (broadcast)", false},
        {"Rainy (broadcast)", false},
        {"Stormy (broadcast)", false},
        {"Gift item", true},
        {"Whisper", true},
        {"Mood +20", true},
        {"Mood -20", true},
        {"Force kill", true},
    };
    return items;
}

ftxui::Element intervenePanel(const CreatorSnapshot& s, const std::string& target_name) {
    const auto& actions = interveneActions();
    const bool has_target = !target_name.empty();
    std::vector<ftxui::Element> rows;
    for (int i = 0; i < static_cast<int>(actions.size()); ++i) {
        const auto& a = actions[i];
        const bool disabled = a.needs_target && !has_target;
        auto row = ftxui::text((i == s.intervene_index ? "▶ " : "  ") + a.label);
        if (disabled) {
            row = row | ftxui::dim;
        } else if (i == s.intervene_index) {
            row = row | ftxui::bold | ftxui::color(ftxui::Color::Yellow);
        }
        rows.push_back(row);
    }

    const std::string title = has_target ? " Intervene with " + target_name + " " : " Intervene (no AI selected) ";
    auto help = ftxui::text("↑/↓ select   Enter activate   Esc back") | ftxui::dim;
    auto window = ftxui::window(ftxui::text(title) | ftxui::bold,
                                ftxui::vbox({ftxui::vbox(std::move(rows)), ftxui::separator(), help})) |
                  ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 50);
    return ftxui::clear_under(window);
}

ftxui::Element inputModal(const std::string& title, const std::string& input) {
    auto window = ftxui::window(ftxui::text(" " + title + " ") | ftxui::bold,
                                ftxui::vbox({ftxui::text(input + "_") | ftxui::inverted,
                                             ftxui::text("Enter submit   Esc cancel") | ftxui::dim})) |
                  ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 60);
    return ftxui::clear_under(window);
}

std::string inputTitle(CreatorMode mode) {
    switch (mode) {
    case CreatorMode::InputName:
        return "Name for new being (blank = random unique)";
    case CreatorMode::InputQuirk:
        return "Personality quirk";
    case CreatorMode::InputGift:
        return "Gift item for selected AI";
    case CreatorMode::InputWhisper:
        return "Whisper to selected AI";
    default:
        return "";
    }
}

} // namespace

CreatorUI::CreatorUI(ftxui::ScreenInteractive* screen, CreatorActions actions)
    : screen_{screen}, actions_{std::move(actions)} {}

ftxui::Component CreatorUI::renderer() {
    auto view = ftxui::Renderer([this] {
        std::scoped_lock lock{snapshot_.mutex};

        const auto& presences = snapshot_.presences;
        const int n = static_cast<int>(presences.size());

        std::string scan_time =
            snapshot_.last_scan.time_since_epoch().count() > 0 ? formatTime(snapshot_.last_scan) : "--:--:--";
        auto header = ftxui::hbox({ftxui::text(" AI Life - Creator ") | ftxui::bold, ftxui::filler(),
                                   ftxui::text("presence: " + Paths::presenceDirectory().string() +
                                               "   last scan: " + scan_time) |
                                       ftxui::dim}) |
                      ftxui::borderRounded;

        std::vector<ftxui::Element> list_rows;
        if (n == 0) {
            list_rows.push_back(ftxui::paragraph("no AIs detected. start one with `c` or `./ailife --spawn ...`.") |
                                ftxui::dim);
        } else {
            int selected_partner_pid = 0;
            if (snapshot_.selected_index >= 0 && snapshot_.selected_index < n) {
                selected_partner_pid = presences[snapshot_.selected_index].talking_to_pid;
            }
            for (int i = 0; i < n; ++i) {
                const bool is_selected = i == snapshot_.selected_index;
                const bool is_partner =
                    !is_selected && selected_partner_pid != 0 && presences[i].pid == selected_partner_pid;
                list_rows.push_back(listRow(presences[i], is_selected, is_partner));
            }
        }
        auto list_pane = ftxui::window(ftxui::text(" Beings (" + std::to_string(n) + ") ") | ftxui::bold,
                                       ftxui::vbox(std::move(list_rows))) |
                         ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 60);

        ftxui::Element detail;
        if (snapshot_.selected_index >= 0 && snapshot_.selected_index < n) {
            detail = detailWithTranscript(presences[snapshot_.selected_index], snapshot_.selected_transcript);
        } else {
            detail = ftxui::text("select an AI with ↑/↓") | ftxui::dim | ftxui::center | ftxui::flex;
        }
        auto detail_pane = ftxui::window(ftxui::text(" Detail ") | ftxui::bold, std::move(detail)) | ftxui::flex;

        ftxui::Element body = ftxui::hbox({std::move(list_pane), std::move(detail_pane)});
        if (!snapshot_.last_error.empty()) {
            body = ftxui::vbox({body, ftxui::text("error: " + snapshot_.last_error) | ftxui::color(ftxui::Color::Red)});
        }
        if (!snapshot_.status.empty()) {
            body = ftxui::vbox({body, ftxui::text(snapshot_.status) | ftxui::color(ftxui::Color::GreenLight)});
        }

        auto footer =
            ftxui::hbox({ftxui::text("↑/↓ select   c create   i intervene   r refresh   q quit") | ftxui::dim}) |
            ftxui::borderRounded;

        auto base = ftxui::vbox({std::move(header), std::move(body) | ftxui::flex, std::move(footer)});

        ftxui::Element overlay;
        switch (snapshot_.mode) {
        case CreatorMode::Spawn:
            overlay = spawnStudio(snapshot_);
            break;
        case CreatorMode::Intervene: {
            std::string target_name;
            if (snapshot_.selected_index >= 0 && snapshot_.selected_index < n) {
                target_name = presences[snapshot_.selected_index].name;
            }
            overlay = intervenePanel(snapshot_, target_name);
            break;
        }
        case CreatorMode::InputName:
        case CreatorMode::InputQuirk:
        case CreatorMode::InputGift:
        case CreatorMode::InputWhisper:
            overlay = inputModal(inputTitle(snapshot_.mode), snapshot_.input);
            break;
        case CreatorMode::List:
        default:
            break;
        }

        if (overlay) {
            return ftxui::dbox({base | ftxui::dim, overlay | ftxui::center});
        }
        return base;
    });

    return ftxui::CatchEvent(view, [this](const ftxui::Event& event) {
        CreatorMode mode;
        {
            std::scoped_lock lock{snapshot_.mutex};
            mode = snapshot_.mode;
        }
        switch (mode) {
        case CreatorMode::Spawn:
            return handleSpawnEvent(event);
        case CreatorMode::Intervene:
            return handleInterveneEvent(event);
        case CreatorMode::InputName:
        case CreatorMode::InputQuirk:
        case CreatorMode::InputGift:
        case CreatorMode::InputWhisper:
            return handleInputEvent(event);
        case CreatorMode::List:
        default:
            return handleListEvent(event);
        }
    });
}

void CreatorUI::setPresences(std::vector<PresenceSnapshot> presences, std::chrono::system_clock::time_point at) {
    {
        std::scoped_lock lock{snapshot_.mutex};
        snapshot_.presences = std::move(presences);
        snapshot_.last_scan = at;

        const auto& p = snapshot_.presences;
        const int n = static_cast<int>(p.size());
        snapshot_.selected_index = -1;
        if (snapshot_.selected_pid != 0) {
            for (int i = 0; i < n; ++i) {
                if (p[i].pid == snapshot_.selected_pid) {
                    snapshot_.selected_index = i;
                    break;
                }
            }
            if (snapshot_.selected_index < 0) {
                snapshot_.selected_pid = 0;
            }
        }
        if (snapshot_.selected_index < 0 && n > 0) {
            snapshot_.selected_index = 0;
            snapshot_.selected_pid = p[0].pid;
        }
    }
    wake();
}

void CreatorUI::setSelectedTranscript(std::vector<Message> messages) {
    {
        std::scoped_lock lock{snapshot_.mutex};
        snapshot_.selected_transcript = std::move(messages);
    }
    wake();
}

int CreatorUI::selectedPid() {
    std::scoped_lock lock{snapshot_.mutex};
    return snapshot_.selected_pid;
}

void CreatorUI::setError(std::string message) {
    {
        std::scoped_lock lock{snapshot_.mutex};
        snapshot_.last_error = std::move(message);
    }
    wake();
}

void CreatorUI::wake() {
    if (screen_ != nullptr) {
        screen_->PostEvent(ftxui::Event::Custom);
    }
}

void CreatorUI::moveSelection(int delta) {
    std::scoped_lock lock{snapshot_.mutex};
    const int n = static_cast<int>(snapshot_.presences.size());
    if (n == 0) {
        return;
    }
    const int current = snapshot_.selected_index < 0 ? 0 : snapshot_.selected_index;
    snapshot_.selected_index = std::clamp(current + delta, 0, n - 1);
    snapshot_.selected_pid = snapshot_.presences[snapshot_.selected_index].pid;
}

int CreatorUI::selectedPidLocked() const {
    return snapshot_.selected_pid;
}

bool CreatorUI::handleListEvent(const ftxui::Event& event) {
    if (event == ftxui::Event::Character('c') || event == ftxui::Event::Character('C')) {
        std::scoped_lock lock{snapshot_.mutex};
        snapshot_.mode = CreatorMode::Spawn;
        return true;
    }
    if (event == ftxui::Event::Character('i') || event == ftxui::Event::Character('I')) {
        std::scoped_lock lock{snapshot_.mutex};
        snapshot_.mode = CreatorMode::Intervene;
        snapshot_.intervene_index = 0;
        return true;
    }
    if (event == ftxui::Event::ArrowUp || event == ftxui::Event::Character('k')) {
        moveSelection(-1);
        return true;
    }
    if (event == ftxui::Event::ArrowDown || event == ftxui::Event::Character('j')) {
        moveSelection(1);
        return true;
    }
    if (event == ftxui::Event::Character('r') || event == ftxui::Event::Character('R')) {
        wake();
        return true;
    }
    return false;
}

bool CreatorUI::handleSpawnEvent(const ftxui::Event& event) {
    if (event == ftxui::Event::Escape) {
        std::scoped_lock lock{snapshot_.mutex};
        snapshot_.mode = CreatorMode::List;
        return true;
    }
    if (event == ftxui::Event::ArrowUp) {
        std::scoped_lock lock{snapshot_.mutex};
        snapshot_.spawn_field_index = (snapshot_.spawn_field_index - 1 + kFieldCount) % kFieldCount;
        return true;
    }
    if (event == ftxui::Event::ArrowDown) {
        std::scoped_lock lock{snapshot_.mutex};
        snapshot_.spawn_field_index = (snapshot_.spawn_field_index + 1) % kFieldCount;
        return true;
    }
    if (event == ftxui::Event::ArrowLeft) {
        adjustSpawnField(-1);
        return true;
    }
    if (event == ftxui::Event::ArrowRight) {
        adjustSpawnField(1);
        return true;
    }
    if (event == ftxui::Event::Character('r') || event == ftxui::Event::Character('R')) {
        randomizeSpawn();
        return true;
    }
    if (event == ftxui::Event::Character('s') || event == ftxui::Event::Character('S')) {
        triggerSpawn();
        return true;
    }
    if (event == ftxui::Event::Return) {
        int field;
        {
            std::scoped_lock lock{snapshot_.mutex};
            field = snapshot_.spawn_field_index;
        }
        if (field == kFieldName) {
            std::scoped_lock lock{snapshot_.mutex};
            snapshot_.mode = CreatorMode::InputName;
            snapshot_.input = snapshot_.spawn_name;
        } else if (field == kFieldQuirk) {
            std::scoped_lock lock{snapshot_.mutex};
            snapshot_.mode = CreatorMode::InputQuirk;
            snapshot_.input = snapshot_.spawn_personality.quirk;
        } else if (field == kFieldSpawnButton) {
            triggerSpawn();
        } else {
            adjustSpawnField(1);
        }
        return true;
    }
    return true;
}

void CreatorUI::adjustSpawnField(int delta) {
    std::scoped_lock lock{snapshot_.mutex};
    switch (snapshot_.spawn_field_index) {
    case kFieldArchetype: {
        static const std::vector<std::string> archetypes{"curious", "cautious", "warm", "gloomy"};
        auto it = std::find(archetypes.begin(), archetypes.end(), snapshot_.spawn_archetype);
        const int size = static_cast<int>(archetypes.size());
        int idx = it == archetypes.end() ? 0 : static_cast<int>(it - archetypes.begin());
        idx = ((idx + delta) % size + size) % size;
        snapshot_.spawn_archetype = archetypes[idx];
        snapshot_.spawn_personality = personalityForArchetype(snapshot_.spawn_archetype);
        break;
    }
    case kFieldLifespan:
        snapshot_.spawn_lifespan_minutes =
            std::clamp(snapshot_.spawn_lifespan_minutes + static_cast<double>(delta), 1.0, 60.0);
        break;
    case kFieldHat: {
        const int size = static_cast<int>(availableHatAssets().size());
        snapshot_.spawn_hat_index = ((static_cast<int>(snapshot_.spawn_hat_index) + delta) % size + size) % size;
        break;
    }
    case kFieldEyes: {
        const int size = static_cast<int>(availableEyeAssets().size());
        snapshot_.spawn_eyes_index = ((static_cast<int>(snapshot_.spawn_eyes_index) + delta) % size + size) % size;
        break;
    }
    case kFieldMouth: {
        const int size = static_cast<int>(availableMouthAssets().size());
        snapshot_.spawn_mouth_index = ((static_cast<int>(snapshot_.spawn_mouth_index) + delta) % size + size) % size;
        break;
    }
    case kFieldOpenness:
        clampTrait(snapshot_.spawn_personality.openness, 10 * delta);
        break;
    case kFieldConscientiousness:
        clampTrait(snapshot_.spawn_personality.conscientiousness, 10 * delta);
        break;
    case kFieldExtraversion:
        clampTrait(snapshot_.spawn_personality.extraversion, 10 * delta);
        break;
    case kFieldAgreeableness:
        clampTrait(snapshot_.spawn_personality.agreeableness, 10 * delta);
        break;
    case kFieldNeuroticism:
        clampTrait(snapshot_.spawn_personality.neuroticism, 10 * delta);
        break;
    default:
        break;
    }
}

void CreatorUI::randomizeSpawn() {
    static thread_local std::mt19937 rng{std::random_device{}()};
    static const std::vector<std::string> archetypes{"curious", "cautious", "warm", "gloomy"};
    std::scoped_lock lock{snapshot_.mutex};
    std::uniform_int_distribution<int> arche(0, static_cast<int>(archetypes.size()) - 1);
    snapshot_.spawn_archetype = archetypes[arche(rng)];
    snapshot_.spawn_personality = personalityForArchetype(snapshot_.spawn_archetype);
    std::uniform_int_distribution<int> jitter(-15, 15);
    auto j = [&](int& v) { v = std::clamp(v + jitter(rng), 0, 100); };
    j(snapshot_.spawn_personality.openness);
    j(snapshot_.spawn_personality.conscientiousness);
    j(snapshot_.spawn_personality.extraversion);
    j(snapshot_.spawn_personality.agreeableness);
    j(snapshot_.spawn_personality.neuroticism);
    auto pick = [&](std::size_t n) {
        std::uniform_int_distribution<std::size_t> d(0, n - 1);
        return d(rng);
    };
    snapshot_.spawn_hat_index = pick(availableHatAssets().size());
    snapshot_.spawn_eyes_index = pick(availableEyeAssets().size());
    snapshot_.spawn_mouth_index = pick(availableMouthAssets().size());
    snapshot_.spawn_name.clear();
}

void CreatorUI::triggerSpawn() {
    SpawnOptions options;
    {
        std::scoped_lock lock{snapshot_.mutex};
        options.name = snapshot_.spawn_name;
        options.archetype = snapshot_.spawn_archetype;
        options.lifespan_minutes = snapshot_.spawn_lifespan_minutes;
        options.appearance = {availableHatAssets()[snapshot_.spawn_hat_index],
                              availableEyeAssets()[snapshot_.spawn_eyes_index],
                              availableMouthAssets()[snapshot_.spawn_mouth_index]};
        options.custom_personality = true;
        options.personality = snapshot_.spawn_personality;
        snapshot_.mode = CreatorMode::List;
    }
    try {
        if (actions_.spawnConfigured) {
            actions_.spawnConfigured(options);
        }
        setError("");
    } catch (const std::exception& ex) {
        setError(ex.what());
    }
}

bool CreatorUI::handleInterveneEvent(const ftxui::Event& event) {
    const int n = static_cast<int>(interveneActions().size());
    if (event == ftxui::Event::Escape) {
        std::scoped_lock lock{snapshot_.mutex};
        snapshot_.mode = CreatorMode::List;
        return true;
    }
    if (event == ftxui::Event::ArrowUp || event == ftxui::Event::Character('k')) {
        std::scoped_lock lock{snapshot_.mutex};
        snapshot_.intervene_index = (snapshot_.intervene_index - 1 + n) % n;
        return true;
    }
    if (event == ftxui::Event::ArrowDown || event == ftxui::Event::Character('j')) {
        std::scoped_lock lock{snapshot_.mutex};
        snapshot_.intervene_index = (snapshot_.intervene_index + 1) % n;
        return true;
    }
    if (event == ftxui::Event::Return) {
        activateIntervene();
        return true;
    }
    return true;
}

void CreatorUI::activateIntervene() {
    int idx = 0;
    int target_pid = 0;
    {
        std::scoped_lock lock{snapshot_.mutex};
        idx = snapshot_.intervene_index;
        target_pid = snapshot_.selected_pid;
    }
    const auto& actions = interveneActions();
    if (actions[idx].needs_target && target_pid == 0) {
        setError("select an AI first");
        std::scoped_lock lock{snapshot_.mutex};
        snapshot_.mode = CreatorMode::List;
        return;
    }

    // gift and whisper require additional input
    if (idx == 5) {
        std::scoped_lock lock{snapshot_.mutex};
        snapshot_.mode = CreatorMode::InputGift;
        snapshot_.input.clear();
        return;
    }
    if (idx == 6) {
        std::scoped_lock lock{snapshot_.mutex};
        snapshot_.mode = CreatorMode::InputWhisper;
        snapshot_.input.clear();
        return;
    }

    try {
        switch (idx) {
        case 0:
            if (actions_.weather)
                actions_.weather("sunny", 1);
            break;
        case 1:
            if (actions_.weather)
                actions_.weather("breezy", 1);
            break;
        case 2:
            if (actions_.weather)
                actions_.weather("clear night", 1);
            break;
        case 3:
            if (actions_.weather)
                actions_.weather("rainy", 2);
            break;
        case 4:
            if (actions_.weather)
                actions_.weather("stormy", 3);
            break;
        case 7:
            if (actions_.moodNudge)
                actions_.moodNudge(target_pid, 20.0F);
            break;
        case 8:
            if (actions_.moodNudge)
                actions_.moodNudge(target_pid, -20.0F);
            break;
        case 9:
            if (actions_.kill)
                actions_.kill(target_pid);
            break;
        default:
            break;
        }
        setError("");
    } catch (const std::exception& ex) {
        setError(ex.what());
    }
    std::scoped_lock lock{snapshot_.mutex};
    snapshot_.mode = CreatorMode::List;
}

bool CreatorUI::handleInputEvent(const ftxui::Event& event) {
    CreatorMode mode;
    int target_pid = 0;
    std::string value;
    {
        std::scoped_lock lock{snapshot_.mutex};
        mode = snapshot_.mode;
        target_pid = selectedPidLocked();
        if (event == ftxui::Event::Escape) {
            snapshot_.mode = (mode == CreatorMode::InputName || mode == CreatorMode::InputQuirk) ? CreatorMode::Spawn
                                                                                                 : CreatorMode::List;
            snapshot_.input.clear();
            return true;
        }
        if (event == ftxui::Event::Backspace) {
            if (!snapshot_.input.empty()) {
                snapshot_.input.pop_back();
            }
            return true;
        }
        if (event == ftxui::Event::Return) {
            value = snapshot_.input;
            if (mode == CreatorMode::InputName) {
                snapshot_.spawn_name = value;
                snapshot_.mode = CreatorMode::Spawn;
            } else if (mode == CreatorMode::InputQuirk) {
                if (!value.empty()) {
                    snapshot_.spawn_personality.quirk = value;
                }
                snapshot_.mode = CreatorMode::Spawn;
            } else {
                snapshot_.mode = CreatorMode::List;
            }
            snapshot_.input.clear();
        } else if (event.is_character()) {
            const std::string ch = event.character();
            if (!ch.empty() && static_cast<unsigned char>(ch[0]) >= 32 && static_cast<unsigned char>(ch[0]) < 127 &&
                snapshot_.input.size() < 80) {
                snapshot_.input += ch;
            }
            return true;
        } else {
            return true;
        }
    }

    try {
        if (mode == CreatorMode::InputGift && target_pid != 0 && !value.empty() && actions_.gift) {
            actions_.gift(target_pid, value);
            setError("");
        } else if (mode == CreatorMode::InputWhisper && target_pid != 0 && !value.empty() && actions_.whisper) {
            actions_.whisper(target_pid, value);
            setError("");
        }
    } catch (const std::exception& ex) {
        setError(ex.what());
    }
    return true;
}
