#ifndef AILIFE_SIMULATION_H
#define AILIFE_SIMULATION_H

#include "ai_character.h"
#include "cli.h"
#include "conversation_manager.h"
#include "decision_strategy.h"
#include "llm_client.h"
#include "logger.h"
#include "presence_registry.h"
#include "simulation_clock.h"
#include "simulation_observer.h"

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <vector>

class Simulation {
  public:
    explicit Simulation(Cli::Config config);
    void run();
    void addObserver(SimulationObserver* observer);
    void requestStop();

  private:
    void tickOnce();
    void applyBodyAction(const Action& action);
    void dispatchReply(const Action& action, const TickContext& ctx);
    void dispatchEndChat(const Action& action);
    void dispatchIgnore(const TickContext& ctx);
    void dispatchInitiateChat(const Action& action);
    void onStageChanged(LifeStage previous, LifeStage current);
    void onDeath();
    std::filesystem::path writeMemoriesFile(const std::string& markdown) const;
    std::string buildLifeLog() const;

    bool eligibleToChat() const;
    const PresenceSnapshot* chooseInitiationTarget() const;
    void recordRelationshipExchange(int partner_pid, const std::string& partner_name, Tone tone, bool receiver_side);
    void notifyConversationEnded(int partner_pid, EndReason reason);

    AICharacter ai_;
    std::unique_ptr<LLMClient> llm_;
    std::unique_ptr<DecisionStrategy> strategy_;
    SimulationClock clock_;
    Logger logger_;
    std::vector<SimulationObserver*> observers_;
    std::mutex stop_mutex_;
    std::condition_variable stop_cv_;
    std::atomic<bool> stop_requested_{false};
    bool finished_{false};

    int my_pid_{0};
    std::chrono::system_clock::time_point birth_at_{std::chrono::system_clock::now()};
    ConversationManager conversation_;
    PresenceReader neighbors_;
    std::vector<PresenceSnapshot> latest_neighbors_;
};

#endif // AILIFE_SIMULATION_H
