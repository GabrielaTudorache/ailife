#include "creator_ui.h"

#include "paths.h"
#include "ui_widgets.h"

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
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
            // focus the last message
            if (i == last_idx) {
                block = block | ftxui::focus;
            }
            lines.push_back(std::move(block));
        }
    }

    const std::string prefix = active ? "Active conversation" : "Last conversation";
    const std::string title =
        " " + prefix + " with " + partner + "  (" + std::to_string(messages.size()) + " msgs) ";
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

// cap activity entries when transcript is visible
constexpr int kActivityRowsWithTranscript = 10;

ftxui::Element detailPanel(const PresenceSnapshot& snap, bool transcript_visible) {
    const auto age =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - snap.last_heartbeat);

    auto mascot = UIWidgets::mascotBox(snap.stage, snap.mood, snap.last_action);

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

} // namespace

CreatorUI::CreatorUI(ftxui::ScreenInteractive* screen) : screen_{screen} {}

ftxui::Component CreatorUI::renderer() {
    auto view = ftxui::Renderer([this] {
        std::scoped_lock lock{snapshot_.mutex};

        const auto& presences = snapshot_.presences;
        const int n = static_cast<int>(presences.size());

        // header
        std::string scan_time =
            snapshot_.last_scan.time_since_epoch().count() > 0 ? formatTime(snapshot_.last_scan) : "--:--:--";
        auto header = ftxui::hbox({ftxui::text(" AI Life - Creator ") | ftxui::bold, ftxui::filler(),
                                   ftxui::text("presence: " + Paths::presenceDirectory().string() +
                                               "   last scan: " + scan_time) |
                                       ftxui::dim}) |
                      ftxui::borderRounded;

        // list pane
        std::vector<ftxui::Element> list_rows;
        if (n == 0) {
            list_rows.push_back(
                ftxui::paragraph("no AIs detected. start one with `./ailife --spawn ...` in another terminal.") |
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

        // detail pane
        ftxui::Element detail;
        if (snapshot_.selected_index >= 0 && snapshot_.selected_index < n) {
            detail = detailWithTranscript(presences[snapshot_.selected_index], snapshot_.selected_transcript);
        } else {
            detail = ftxui::text("select an AI with ↑/↓") | ftxui::dim | ftxui::center | ftxui::flex;
        }
        auto detail_pane = ftxui::window(ftxui::text(" Detail ") | ftxui::bold, std::move(detail)) | ftxui::flex;

        // error line (only when set)
        ftxui::Element body = ftxui::hbox({std::move(list_pane), std::move(detail_pane)});
        if (!snapshot_.last_error.empty()) {
            body = ftxui::vbox({body, ftxui::text("error: " + snapshot_.last_error) | ftxui::color(ftxui::Color::Red)});
        }

        // footer
        auto footer = ftxui::hbox({ftxui::text("↑/↓ select   r refresh   q quit") | ftxui::dim}) | ftxui::borderRounded;

        return ftxui::vbox({std::move(header), std::move(body) | ftxui::flex, std::move(footer)});
    });

    return ftxui::CatchEvent(view, [this](const ftxui::Event& event) {
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
    });
}

void CreatorUI::setPresences(std::vector<PresenceSnapshot> presences, std::chrono::system_clock::time_point at) {
    {
        std::scoped_lock lock{snapshot_.mutex};
        snapshot_.presences = std::move(presences);
        snapshot_.last_scan = at;

        // Restore selection by PID, or default to first entry
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
