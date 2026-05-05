#ifndef AILIFE_PROMPT_BUILDER_H
#define AILIFE_PROMPT_BUILDER_H

#include "llm_client.h"

class AICharacter;
class SimulationClock;

namespace PromptBuilder {
LLMRequest forTick(const AICharacter& character, const SimulationClock& clock, const std::string& archetype);
LLMRequest forLastWords(const AICharacter& character, const std::string& archetype);
LLMRequest forMemoriesFile(const AICharacter& character, const std::string& archetype);
} // namespace PromptBuilder

#endif // AILIFE_PROMPT_BUILDER_H
