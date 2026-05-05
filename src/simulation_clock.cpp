#include "simulation_clock.h"

SimulationClock::SimulationClock(std::chrono::seconds lifespan, std::chrono::seconds tick_interval)
    : lifespan_(lifespan), tick_interval_(tick_interval) {
    if (lifespan_ <= std::chrono::seconds::zero()) {
        throw std::invalid_argument("Lifespan must be positive");
    }

    if (tick_interval_ <= std::chrono::seconds::zero()) {
        throw std::invalid_argument("Tick interval must be positive");
    }
}

std::chrono::steady_clock::duration SimulationClock::elapsed() const {
    return std::chrono::steady_clock::now() - start_time_;
}

std::chrono::steady_clock::duration SimulationClock::remaining() const {
    const auto lived = elapsed();
    if (lived >= lifespan_) {
        return std::chrono::steady_clock::duration::zero();
    }
    return lifespan_ - lived;
}

double SimulationClock::fractionLived() const {
    const double total = std::chrono::duration<double>(lifespan_).count();
    return std::chrono::duration<double>(elapsed()).count() / total;
}

std::chrono::seconds SimulationClock::getTickInterval() const {
    return tick_interval_;
}
