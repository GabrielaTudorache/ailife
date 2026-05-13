#ifndef AILIFE_DECISION_STRATEGY_H
#define AILIFE_DECISION_STRATEGY_H

#include "action.h"
#include "enums.h"
#include "tick_context.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

class AICharacter;
class LLMClient;
class SimulationClock;

struct TurnResult {
    std::vector<Action> actions;
    std::string thoughts;
};

class DecisionStrategy {
  public:
    virtual ~DecisionStrategy() = default;
    virtual std::string archetypeName() const = 0;
    virtual TurnResult decide(const AICharacter& character, const SimulationClock& clock, LLMClient& llm,
                              const TickContext& ctx);

  protected:
    virtual ActionKind fallbackAction(const AICharacter& character) const;
};

class CuriousExplorerStrategy : public DecisionStrategy {
  public:
    std::string archetypeName() const override;

  protected:
    ActionKind fallbackAction(const AICharacter& character) const override;
};

class CautiousIntrovertStrategy : public DecisionStrategy {
  public:
    std::string archetypeName() const override;

  protected:
    ActionKind fallbackAction(const AICharacter& character) const override;
};

class WarmSocialStrategy : public DecisionStrategy {
  public:
    std::string archetypeName() const override;

  protected:
    ActionKind fallbackAction(const AICharacter& character) const override;
};

class GloomyPhilosopherStrategy : public DecisionStrategy {
  public:
    std::string archetypeName() const override;

  protected:
    ActionKind fallbackAction(const AICharacter& character) const override;
};

std::unique_ptr<DecisionStrategy> makeStrategy(std::string_view archetype);

#endif // AILIFE_DECISION_STRATEGY_H
