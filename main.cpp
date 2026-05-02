#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

int main() {
    using namespace ftxui;

    auto screen = ScreenInteractive::Fullscreen();

    auto renderer = Renderer([&] {
        auto [width, height] = Terminal::Size();

        return vbox({
                   text(""),
                   text(" AI Life ") | bold | color(Color::Cyan) | center,
                   text(""),
                   separator(),
                   text(""),
                   text("  Dimensiuni: " + std::to_string(width) + "x" + std::to_string(height)) | color(Color::Green),
                   text("  Apasa 'q' pentru a iesi.") | color(Color::Yellow),
                   text(""),
               }) |
               border;
    });

    auto component = CatchEvent(renderer, [&](Event event) {
        if (event == Event::Character('q')) {
            screen.ExitLoopClosure()();
            return true;
        }
        return false;
    });

    screen.Loop(component);

    return 0;
}
