#include "simulation.h"

#include "paths.h"
#include "prompt_builder.h"
#include "simulation_exception.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
Personality personalityFor(const std::string& archetype) {
    if (archetype == "curious") {
        return {90, 50, 55, 65, 35, "collects questions"};
    }
    if (archetype == "cautious") {
        return {45, 85, 25, 60, 55, "counts exits"};
    }
    if (archetype == "warm") {
        return {60, 55, 85, 90, 30, "remembers voices"};
    }
    return {75, 50, 30, 55, 80, "names the shadows"};
}

std::unique_ptr<LLMClient> makeLlm(bool mock) {
    if (mock) {
        return std::make_unique<MockLLMClient>();
    }
    const char* api_key = std::getenv("OPENROUTER_API_KEY");
    if (api_key == nullptr || std::string{api_key}.empty()) {
        throw std::runtime_error("OPENROUTER_API_KEY is required unless --mock-llm is set");
    }
    const char* model = std::getenv("OPENROUTER_MODEL");
    return std::make_unique<RealLLMClient>(api_key, model == nullptr ? "google/gemma-4-31b-it" : model);
}

std::string safeFilePart(std::string value) {
    std::replace_if(
        value.begin(), value.end(), [](unsigned char ch) { return !std::isalnum(ch) && ch != '-' && ch != '_'; }, '_');
    return value;
}
} // namespace

Simulation::Simulation(Cli::Config config)
    : ai_(config.name, personalityFor(config.archetype)), llm_(makeLlm(config.mock_llm)),
      strategy_(makeStrategy(config.archetype)), clock_(config.duration) {
    ai_.setLifespan(config.duration);
}

void Simulation::run() {
    logger_.event("TICK", ai_.getName() + " begins a " + strategy_->archetypeName() + " life.");
    for (SimulationObserver* observer : observers_) {
        observer->onSimulationStarted(ai_, strategy_->archetypeName());
    }

    while (!finished_ && !ai_.isDead() && !stop_requested_.load()) {
        tickOnce();
        if (!finished_ && !ai_.isDead() && !stop_requested_.load()) {
            const auto sleep_for =
                std::min<std::chrono::steady_clock::duration>(clock_.getTickInterval(), clock_.remaining());
            if (sleep_for > std::chrono::seconds{0}) {
                // sleep until the next tick, but wake up early if stop is requested
                // this_thread::sleep_for wouldn't respond to requestStop() until the interval elapsed
                std::unique_lock lock{stop_mutex_};
                stop_cv_.wait_for(lock, sleep_for, [&] { return stop_requested_.load(); });
            }
        }
    }
    onDeath();
}

void Simulation::addObserver(SimulationObserver* observer) {
    observers_.push_back(observer);
}

void Simulation::requestStop() {
    stop_requested_.store(true);
    stop_cv_.notify_all();
}

void Simulation::tickOnce() {
    const LifeStage previous = ai_.getLifeStage();
    ai_.tick();
    onStageChanged(previous, ai_.getLifeStage());
    logger_.event("TICK", "hunger=" + std::to_string(ai_.getHunger().getValue()) +
                              " energy=" + std::to_string(ai_.getEnergy().getValue()) +
                              " mood=" + std::to_string(static_cast<int>(ai_.getMood().getValue())) +
                              " loneliness=" + std::to_string(ai_.getLoneliness().getValue()));
    for (SimulationObserver* observer : observers_) {
        observer->onTick(ai_, std::chrono::duration_cast<std::chrono::seconds>(clock_.elapsed()),
                         std::chrono::duration_cast<std::chrono::seconds>(clock_.remaining()));
    }

    Action action{ActionKind::WriteJournal, "Another quiet hour passed, and I carried it with me.", {}};
    try {
        logger_.event("LLM", "asking " + ai_.getName() + " what to do next");
        action = strategy_->decide(ai_, clock_, *llm_);
    } catch (const SimulationException& error) {
        logger_.event("ERROR", error.what());
        for (SimulationObserver* observer : observers_) {
            observer->onError(error.what());
        }
    }

    if (!action.thoughts.empty()) {
        logger_.event("THINK", action.thoughts);
        for (SimulationObserver* observer : observers_) {
            observer->onThought(action.thoughts);
        }
    }
    logger_.event("DECIDE", actionName(action.kind));
    for (SimulationObserver* observer : observers_) {
        observer->onDecision(action.kind);
    }
    applyAction(action);
}

void Simulation::applyAction(const Action& action) {
    ai_.apply(action);
    logger_.event("ACT", ai_.getName() + " chose " + actionName(action.kind));
    for (SimulationObserver* observer : observers_) {
        observer->onAction(ai_, action);
    }
    if (action.kind == ActionKind::WriteJournal && !action.narrative.empty()) {
        logger_.event("JOURNAL", action.narrative);
        for (SimulationObserver* observer : observers_) {
            observer->onJournal(action.narrative);
        }
    }
    if (action.kind == ActionKind::Talk && !action.narrative.empty()) {
        logger_.event("SAY", action.narrative);
        for (SimulationObserver* observer : observers_) {
            observer->onSpoke(action.narrative);
        }
    }
    if (action.kind == ActionKind::SayGoodbye) {
        finished_ = true;
    }
}

void Simulation::onStageChanged(LifeStage previous, LifeStage current) {
    if (previous != current) {
        logger_.event("STAGE", lifeStageName(previous) + " -> " + lifeStageName(current));
        for (SimulationObserver* observer : observers_) {
            observer->onStageChanged(previous, current);
        }
    }
}

void Simulation::onDeath() {
    std::string last_words = "I was here, briefly, and it mattered.";
    try {
        last_words = llm_->complete(PromptBuilder::forLastWords(ai_, strategy_->archetypeName())).content;
    } catch (const SimulationException& error) {
        logger_.event("ERROR", error.what());
        for (SimulationObserver* observer : observers_) {
            observer->onError(error.what());
        }
    }
    ai_.apply(Action{ActionKind::SayGoodbye, last_words, {}});
    logger_.event("DEATH", last_words);
    for (SimulationObserver* observer : observers_) {
        observer->onDeath(last_words);
    }

    std::string memories = "# Memories\n\nI lived a short life and left these words behind.\n";
    try {
        memories = llm_->complete(PromptBuilder::forMemoriesFile(ai_, strategy_->archetypeName())).content;
    } catch (const SimulationException& error) {
        logger_.event("ERROR", error.what());
        for (SimulationObserver* observer : observers_) {
            observer->onError(error.what());
        }
    }
    const auto path = writeMemoriesFile(memories);
    logger_.event("DEATH", "wrote " + path.string());
    for (SimulationObserver* observer : observers_) {
        observer->onMemoriesWritten(path);
    }
}

std::filesystem::path Simulation::writeMemoriesFile(const std::string& markdown) const {
    const auto path = Paths::memoriesDirectory() /
                      ("memories_" + safeFilePart(ai_.getName()) + '_' + std::to_string(ai_.getId()) + ".md");
    std::ofstream out{path};
    if (!out) {
        throw std::runtime_error("could not write memories file: " + path.string());
    }
    out << markdown << '\n';
    return path;
}
