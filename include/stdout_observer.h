#ifndef AILIFE_STDOUT_OBSERVER_H
#define AILIFE_STDOUT_OBSERVER_H

#include "logger.h"
#include "simulation_observer.h"

#include <string>

class StdoutObserver : public SimulationObserver {
  public:
    void onSimulationStarted(const AICharacter& character, const std::string& archetype) override;
    void onTick(const AICharacter& character, std::chrono::seconds elapsed, std::chrono::seconds remaining) override;
    void onThought(const std::string& text) override;
    void onDecision(ActionKind kind) override;
    void onAction(const AICharacter& character, const Action& action) override;
    void onJournal(const std::string& text) override;
    void onSpoke(const std::string& text) override;
    void onStageChanged(LifeStage prev, LifeStage curr) override;
    void onError(const std::string& msg) override;
    void onDeath(const std::string& text) override;
    void onMemoriesWritten(const std::filesystem::path& path) override;
    void onConversationStarted(int partner_pid, const std::string& partner_name) override;
    void onConversationMessage(int partner_pid, int sequence, const Message& message, bool is_outgoing) override;
    void onConversationEnded(int partner_pid, EndReason reason) override;

  private:
    Logger logger_;
};

#endif // AILIFE_STDOUT_OBSERVER_H
