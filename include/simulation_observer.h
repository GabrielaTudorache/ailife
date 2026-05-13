#ifndef AILIFE_SIMULATION_OBSERVER_H
#define AILIFE_SIMULATION_OBSERVER_H

#include "action.h"
#include "ai_character.h"
#include "conversation.h"

#include <chrono>
#include <filesystem>
#include <string>

class SimulationObserver {
  public:
    virtual ~SimulationObserver() = default;

    virtual void onSimulationStarted(const AICharacter&, const std::string&) {}
    virtual void onTick(const AICharacter&, std::chrono::seconds, std::chrono::seconds) {}
    virtual void onThought(const std::string&) {}
    virtual void onDecision(ActionKind) {}
    virtual void onAction(const AICharacter&, const Action&) {}
    virtual void onJournal(const std::string&) {}
    virtual void onSpoke(const std::string&) {}
    virtual void onStageChanged(LifeStage, LifeStage) {}
    virtual void onError(const std::string&) {}
    virtual void onDeath(const std::string&) {}
    virtual void onMemoriesWritten(const std::filesystem::path&) {}
    virtual void onConversationStarted(int, const std::string&) {}
    virtual void onConversationMessage(int, int, const Message&, bool) {}
    virtual void onConversationEnded(int, EndReason) {}
};

#endif // AILIFE_SIMULATION_OBSERVER_H
