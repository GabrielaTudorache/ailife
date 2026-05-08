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

std::string moodLabel(float mood) {
    if (mood >= 65.0F) {
        return "bright";
    }
    if (mood <= 35.0F) {
        return "low";
    }
    return "steady";
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

std::string clockText(std::chrono::seconds value) {
    const auto total = std::max<long long>(0, value.count());
    std::ostringstream out;
    out << std::setw(2) << std::setfill('0') << total / 60 << ':' << std::setw(2) << std::setfill('0') << total % 60;
    return out.str();
}

ftxui::Element listRow(const PresenceSnapshot& snap, bool selected) {
    std::string row_text = moodDot(snap.mood) + " " + snap.name + " (" + snap.archetype + ")  " +
                           lifeStageName(snap.stage) + "  mood=" + moodLabel(snap.mood) +
                           "  age=" + clockText(snap.elapsed);
    auto elem = ftxui::text(row_text);
    if (selected) {
        elem = elem | ftxui::inverted;
    }
    return elem;
}

ftxui::Element detailPanel(const PresenceSnapshot& snap) {
    const auto age =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - snap.last_heartbeat);

    auto mascot = UIWidgets::mascotBox(snap.stage, snap.mood, snap.last_action);

    std::string header = snap.name + " - " + snap.archetype + "  [" + lifeStageName(snap.stage) + "]";

    auto stats = ftxui::vbox({UIWidgets::statBar("hunger", static_cast<float>(snap.hunger)),
                              UIWidgets::statBar("energy", static_cast<float>(snap.energy)),
                              UIWidgets::statBar("mood", snap.mood),
                              UIWidgets::statBar("loneliness", static_cast<float>(snap.loneliness), false)});

    std::string last_act = actionName(snap.last_action);
    if (!snap.last_narrative.empty()) {
        last_act += " - " + snap.last_narrative;
    }

    return ftxui::vbox({ftxui::hbox({mascot, ftxui::separator(),
                                     ftxui::vbox({ftxui::text(header) | ftxui::bold, ftxui::separator(), stats}) |
                                         ftxui::flex}),
                        ftxui::separator(), ftxui::text("last activity: " + last_act) | ftxui::dim,
                        ftxui::text("heartbeat " + std::to_string(age.count()) + "s ago") | ftxui::dim}) |
           ftxui::flex;
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
            for (int i = 0; i < n; ++i) {
                list_rows.push_back(listRow(presences[i], i == snapshot_.selected_index));
            }
        }
        auto list_pane = ftxui::window(ftxui::text(" Beings (" + std::to_string(n) + ") ") | ftxui::bold,
                                       ftxui::vbox(std::move(list_rows))) |
                         ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 60);

        // detail pane
        ftxui::Element detail;
        if (snapshot_.selected_index >= 0 && snapshot_.selected_index < n) {
            detail = detailPanel(presences[snapshot_.selected_index]);
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
