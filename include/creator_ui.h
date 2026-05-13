#ifndef AILIFE_CREATOR_UI_H
#define AILIFE_CREATOR_UI_H

#include "conversation.h"
#include "presence_registry.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

struct CreatorSnapshot {
    std::mutex mutex;
    std::vector<PresenceSnapshot> presences;
    std::chrono::system_clock::time_point last_scan{};
    int selected_index{-1};
    int selected_pid{0};
    std::string last_error;
    std::vector<Message> selected_transcript;
};

class CreatorUI {
  public:
    explicit CreatorUI(ftxui::ScreenInteractive* screen);

    ftxui::Component renderer();
    void setPresences(std::vector<PresenceSnapshot> snapshot, std::chrono::system_clock::time_point at);
    void setSelectedTranscript(std::vector<Message> messages);
    void setError(std::string message);
    int selectedPid();

  private:
    void wake();
    void moveSelection(int delta);

    ftxui::ScreenInteractive* screen_;
    CreatorSnapshot snapshot_;
};

#endif // AILIFE_CREATOR_UI_H
