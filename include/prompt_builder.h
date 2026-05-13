#ifndef AILIFE_PROMPT_BUILDER_H
#define AILIFE_PROMPT_BUILDER_H

#include "llm_client.h"

#include <string>

class AICharacter;
class SimulationClock;
struct TickContext;

namespace PromptBuilder {
LLMRequest forTurn(const AICharacter& character, const SimulationClock& clock, const std::string& archetype,
                   const TickContext& ctx);
LLMRequest forLastWords(const AICharacter& character, const std::string& archetype);
LLMRequest forMemoriesFile(const AICharacter& character, const std::string& archetype);
} // namespace PromptBuilder

#endif // AILIFE_PROMPT_BUILDER_H
