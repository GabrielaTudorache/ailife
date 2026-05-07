#ifndef AILIFE_AI_CHARACTER_UI_H
#define AILIFE_AI_CHARACTER_UI_H

#include "simulation_observer.h"
#include "ui_snapshot.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <chrono>
#include <filesystem>
#include <string>

class AICharacterUI : public SimulationObserver {
  public:
    explicit AICharacterUI(ftxui::ScreenInteractive* screen);

    ftxui::Component renderer();

    void onSimulationStarted(const AICharacter& character, const std::string& archetype) override;
    void onTick(const AICharacter& character, std::chrono::seconds elapsed, std::chrono::seconds remaining) override;
    void onThought(const std::string& thoughts) override;
    void onDecision(ActionKind kind) override;
    void onAction(const AICharacter& character, const Action& action) override;
    void onJournal(const std::string& narrative) override;
    void onSpoke(const std::string& narrative) override;
    void onStageChanged(LifeStage previous, LifeStage current) override;
    void onError(const std::string& message) override;
    void onDeath(const std::string& last_words) override;
    void onMemoriesWritten(const std::filesystem::path& path) override;

  private:
    void updateStats(const AICharacter& character);
    void appendLine(std::string tag, std::string text, ftxui::Color color);
    void wake();

    ftxui::ScreenInteractive* screen_;
    UISnapshot snapshot_;
    int scroll_target_{-1};
};

#endif // AILIFE_AI_CHARACTER_UI_H
