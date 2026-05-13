#include "decision_strategy.h"

#include "action_spec.h"
#include "ai_character.h"
#include "llm_client.h"
#include "prompt_builder.h"
#include "simulation_exception.h"

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <utility>

namespace {
Action actionFromToolCall(const ToolCall& call) {
    const auto* spec = specForToolName(call.name);
    if (spec == nullptr) {
        throw LLMInvalidResponseException("LLM called unknown tool: " + call.name);
    }
    Action action{spec->kind, {}};
    if (!spec->has_text_param && !spec->has_tone_param) {
        return action;
    }
    nlohmann::json parsed;
    try {
        parsed = nlohmann::json::parse(call.arguments_json);
    } catch (const nlohmann::json::exception&) {
        return action;
    }
    if (spec->has_text_param) {
        const auto it = parsed.find(std::string{spec->text_param_name});
        if (it != parsed.end() && it->is_string()) {
            action.narrative = it->get<std::string>();
        }
    }
    if (spec->has_tone_param) {
        const auto it = parsed.find("tone");
        if (it != parsed.end() && it->is_string()) {
            action.tone = parseTone(it->get<std::string>());
        }
    }
    return action;
}
} // namespace

TurnResult DecisionStrategy::decide(const AICharacter& character, const SimulationClock& clock, LLMClient& llm,
                                    const TickContext& ctx) {
    if (character.getLifeStage() == LifeStage::Dying) {
        return {{Action{ActionKind::SayGoodbye, {}}}, {}};
    }

    const LLMResponse response = llm.complete(PromptBuilder::forTurn(character, clock, archetypeName(), ctx));
    TurnResult result;
    result.thoughts = response.content;
    if (response.tool_calls.empty()) {
        result.actions.push_back({fallbackAction(character), {}});
        return result;
    }
    for (const auto& call : response.tool_calls) {
        result.actions.push_back(actionFromToolCall(call));
    }
    return result;
}

ActionKind DecisionStrategy::fallbackAction(const AICharacter& character) const {
    if (character.getLoneliness().getValue() > 70) {
        return ActionKind::Talk;
    }
    return ActionKind::WriteJournal;
}

std::string CuriousExplorerStrategy::archetypeName() const {
    return "curious";
}

ActionKind CuriousExplorerStrategy::fallbackAction(const AICharacter& character) const {
    if (character.getEnergy().getValue() < 35) {
        return ActionKind::Rest;
    }
    return ActionKind::WriteJournal;
}

std::string CautiousIntrovertStrategy::archetypeName() const {
    return "cautious";
}

ActionKind CautiousIntrovertStrategy::fallbackAction(const AICharacter& character) const {
    if (character.getHunger().getValue() < 45) {
        return ActionKind::Feed;
    }
    return ActionKind::Rest;
}

std::string WarmSocialStrategy::archetypeName() const {
    return "warm";
}

ActionKind WarmSocialStrategy::fallbackAction(const AICharacter& character) const {
    if (character.getLoneliness().getValue() > 35) {
        return ActionKind::Talk;
    }
    return DecisionStrategy::fallbackAction(character);
}

std::string GloomyPhilosopherStrategy::archetypeName() const {
    return "gloomy";
}

ActionKind GloomyPhilosopherStrategy::fallbackAction(const AICharacter& character) const {
    if (character.getMood().getValue() < 25.0F) {
        return ActionKind::Rest;
    }
    return ActionKind::WriteJournal;
}

std::unique_ptr<DecisionStrategy> makeStrategy(std::string_view archetype) {
    if (archetype == "curious") {
        return std::make_unique<CuriousExplorerStrategy>();
    }
    if (archetype == "cautious") {
        return std::make_unique<CautiousIntrovertStrategy>();
    }
    if (archetype == "warm") {
        return std::make_unique<WarmSocialStrategy>();
    }
    if (archetype == "gloomy") {
        return std::make_unique<GloomyPhilosopherStrategy>();
    }
    throw std::invalid_argument("unknown archetype: " + std::string{archetype});
}
