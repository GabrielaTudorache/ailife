#ifndef AILIFE_COMMAND_H
#define AILIFE_COMMAND_H

#include <nlohmann/json.hpp>

#include <memory>
#include <string>

class Simulation;

// world events and creator interventions
class Command {
  public:
    virtual ~Command() = default;
    virtual void apply(Simulation& simulation) const = 0;
    virtual std::string type() const = 0;
    virtual nlohmann::json toJson() const = 0;
    virtual std::string description() const = 0;
    virtual int targetPid() const { return target_pid_; }

  protected:
    explicit Command(int target_pid = 0) : target_pid_{target_pid} {}

  private:
    int target_pid_{0};
};

class WeatherCommand final : public Command {
  public:
    WeatherCommand(std::string condition, int intensity);
    void apply(Simulation& simulation) const override;
    std::string type() const override;
    nlohmann::json toJson() const override;
    std::string description() const override;

  private:
    std::string condition_;
    int intensity_{1};
};

class GiftCommand final : public Command {
  public:
    GiftCommand(int target_pid, std::string item, std::string note = {});
    void apply(Simulation& simulation) const override;
    std::string type() const override;
    nlohmann::json toJson() const override;
    std::string description() const override;

  private:
    std::string item_;
    std::string note_;
};

class WhisperCommand final : public Command {
  public:
    WhisperCommand(int target_pid, std::string message);
    void apply(Simulation& simulation) const override;
    std::string type() const override;
    nlohmann::json toJson() const override;
    std::string description() const override;

  private:
    std::string message_;
};

class MoodNudgeCommand final : public Command {
  public:
    MoodNudgeCommand(int target_pid, float delta);
    void apply(Simulation& simulation) const override;
    std::string type() const override;
    nlohmann::json toJson() const override;
    std::string description() const override;

  private:
    float delta_{0.0F};
};

class KillCommand final : public Command {
  public:
    explicit KillCommand(int target_pid);
    void apply(Simulation& simulation) const override;
    std::string type() const override;
    nlohmann::json toJson() const override;
    std::string description() const override;
};

namespace CommandIo {
std::unique_ptr<Command> fromJson(const nlohmann::json& json);
nlohmann::json envelope(const Command& command);
} // namespace CommandIo

namespace CommandEmitter {
void emit(const Command& command);
} // namespace CommandEmitter

#endif // AILIFE_COMMAND_H
