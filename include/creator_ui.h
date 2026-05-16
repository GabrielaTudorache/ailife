#ifndef AILIFE_CREATOR_UI_H
#define AILIFE_CREATOR_UI_H

#include "conversation.h"
#include "mascot_renderer.h"
#include "presence_registry.h"
#include "spawn_process.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

struct CreatorActions {
    std::function<void(const SpawnOptions&)> spawnConfigured;
    std::function<void(const std::string&, int)> weather;
    std::function<void(int, const std::string&)> gift;
    std::function<void(int, const std::string&)> whisper;
    std::function<void(int, float)> moodNudge;
    std::function<void(int)> kill;
};

enum class CreatorMode {
    List,
    Spawn,
    Intervene,
    InputName,
    InputQuirk,
    InputGift,
    InputWhisper,
};

struct CreatorSnapshot {
    std::mutex mutex;
    std::vector<PresenceSnapshot> presences;
    std::chrono::system_clock::time_point last_scan{};
    int selected_index{-1};
    int selected_pid{0};
    std::string last_error;
    std::vector<Message> selected_transcript;
    CreatorMode mode{CreatorMode::List};
    int spawn_field_index{0};
    int intervene_index{0};
    std::string spawn_name;
    std::string spawn_archetype{"curious"};
    double spawn_lifespan_minutes{8.0};
    std::size_t spawn_hat_index{0};
    std::size_t spawn_eyes_index{0};
    std::size_t spawn_mouth_index{0};
    Personality spawn_personality{90, 50, 55, 65, 35, "collects questions"};
    std::string input;
    std::string status;
};

class CreatorUI {
  public:
    CreatorUI(ftxui::ScreenInteractive* screen, CreatorActions actions);

    ftxui::Component renderer();
    void setPresences(std::vector<PresenceSnapshot> snapshot, std::chrono::system_clock::time_point at);
    void setSelectedTranscript(std::vector<Message> messages);
    void setError(std::string message);
    int selectedPid();

  private:
    void wake();
    void moveSelection(int delta);
    bool handleListEvent(const ftxui::Event& event);
    bool handleSpawnEvent(const ftxui::Event& event);
    bool handleInterveneEvent(const ftxui::Event& event);
    bool handleInputEvent(const ftxui::Event& event);
    void adjustSpawnField(int delta);
    void randomizeSpawn();
    void triggerSpawn();
    void activateIntervene();
    int selectedPidLocked() const;

    ftxui::ScreenInteractive* screen_;
    CreatorActions actions_;
    CreatorSnapshot snapshot_;
};

#endif // AILIFE_CREATOR_UI_H
