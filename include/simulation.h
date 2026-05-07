#ifndef AILIFE_SIMULATION_H
#define AILIFE_SIMULATION_H

#include "ai_character.h"
#include "cli.h"
#include "decision_strategy.h"
#include "llm_client.h"
#include "logger.h"
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
    void applyAction(const Action& action);
    void onStageChanged(LifeStage previous, LifeStage current);
    void onDeath();
    std::filesystem::path writeMemoriesFile(const std::string& markdown) const;

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
};

#endif // AILIFE_SIMULATION_H
