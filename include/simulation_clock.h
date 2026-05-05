#ifndef AILIFE_SIMULATION_CLOCK_H
#define AILIFE_SIMULATION_CLOCK_H

#include <chrono>

class SimulationClock {
  public:
    explicit SimulationClock(std::chrono::seconds lifespan,
                             std::chrono::seconds tick_interval = std::chrono::seconds{2});

    std::chrono::steady_clock::duration elapsed() const;
    std::chrono::steady_clock::duration remaining() const;
    double fractionLived() const;
    std::chrono::seconds getTickInterval() const;

  private:
    std::chrono::steady_clock::time_point start_time_{std::chrono::steady_clock::now()};
    std::chrono::seconds lifespan_;
    std::chrono::seconds tick_interval_;
};

#endif // AILIFE_SIMULATION_CLOCK_H
