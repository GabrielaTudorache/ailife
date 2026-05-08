#ifndef AILIFE_PRESENCE_REGISTRY_H
#define AILIFE_PRESENCE_REGISTRY_H

#include "enums.h"
#include "logger.h"
#include "simulation_observer.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct PresenceSnapshot {
    int pid{};
    int id{};
    std::string name;
    std::string archetype;
    LifeStage stage{LifeStage::Young};
    int hunger{};
    int energy{};
    float mood{};
    int loneliness{};
    std::chrono::seconds elapsed{};
    std::chrono::seconds lifespan{};
    ActionKind last_action{ActionKind::WriteJournal};
    std::string last_narrative;
    std::chrono::system_clock::time_point last_heartbeat;
};

class PresenceWriter : public SimulationObserver {
  public:
    PresenceWriter();
    ~PresenceWriter() override;
    PresenceWriter(const PresenceWriter&) = delete;
    PresenceWriter& operator=(const PresenceWriter&) = delete;

    void start();
    void stop();

    void onSimulationStarted(const AICharacter&, const std::string&) override;
    void onTick(const AICharacter&, std::chrono::seconds, std::chrono::seconds) override;
    void onAction(const AICharacter&, const Action&) override;
    void onJournal(const std::string&) override;
    void onSpoke(const std::string&) override;
    void onStageChanged(LifeStage, LifeStage) override;
    void onDeath(const std::string&) override;

  private:
    void heartbeatLoop();
    void writeOnce();
    void removeFile();

    std::filesystem::path file_path_;
    std::filesystem::path tmp_path_;

    Logger logger_;
    std::mutex snapshot_mutex_;
    PresenceSnapshot snapshot_;

    std::thread thread_;
    std::mutex stop_mutex_;
    std::condition_variable stop_cv_;
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> running_{false};
};

class PresenceReader {
  public:
    explicit PresenceReader(std::chrono::seconds fresh_threshold = std::chrono::seconds{30},
                            std::chrono::hours cleanup_threshold = std::chrono::hours{24});

    std::vector<PresenceSnapshot> scan(Logger& logger) const;

  private:
    std::chrono::seconds fresh_threshold_;
    std::chrono::hours cleanup_threshold_;
};

#endif // AILIFE_PRESENCE_REGISTRY_H
