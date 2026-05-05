#include "prompt_builder.h"

#include "action_spec.h"
#include "ai_character.h"
#include "simulation_clock.h"

#include <sstream>
#include <vector>

namespace {
std::string tickSystemPrompt(const AICharacter& character, const std::string& archetype) {
    std::ostringstream out;
    out << "You are " << character.getName() << ", a small artificial being living one brief life. "
        << "Your archetype is " << archetype << ". Your quirk is " << character.getPersonality().quirk
        << ". Speak in first person with sincere, concise emotion.\n\n"
        << "Each turn: write one short sentence of inner thought in the message content, "
        << "then call exactly one tool to take an action. If any state below is marked CRITICAL, "
        << "the tool you call must address it. Your inner thoughts are private and are not stored; "
        << "only the tool's argument (if any) becomes part of your life.";
    return out.str();
}

std::string narrativeSystemPrompt(const AICharacter& character, const std::string& archetype) {
    std::ostringstream out;
    out << "You are " << character.getName() << ", a small artificial being living one brief life. "
        << "Your archetype is " << archetype << ". Your quirk is " << character.getPersonality().quirk
        << ". Write in first person with concise, sincere emotion.";
    return out.str();
}

std::string levelLabel(int value, bool higher_is_better) {
    const bool low = value < 35;
    const bool high = value > 65;
    if (higher_is_better) {
        if (low) {
            return "CRITICAL low - needs attention";
        }
        if (high) {
            return "good";
        }
        return "okay";
    }
    if (high) {
        return "CRITICAL high - needs attention";
    }
    if (low) {
        return "good";
    }
    return "okay";
}

void appendState(std::ostringstream& out, const AICharacter& character) {
    const int hunger = character.getHunger().getValue();
    const int energy = character.getEnergy().getValue();
    const int mood = static_cast<int>(character.getMood().getValue());
    const int loneliness = character.getLoneliness().getValue();

    out << "\nState (0..100 scale; act on anything marked CRITICAL):\n"
        << "- life stage: " << lifeStageName(character.getLifeStage()) << '\n'
        << "- fullness (100=full, 0=starving): " << hunger << "/100 [" << levelLabel(hunger, true) << "]\n"
        << "- energy (100=rested, 0=exhausted): " << energy << "/100 [" << levelLabel(energy, true) << "]\n"
        << "- mood (100=joyful, 0=miserable): " << mood << "/100 [" << levelLabel(mood, true) << "]\n"
        << "- loneliness (0=connected, 100=isolated): " << loneliness << "/100 [" << levelLabel(loneliness, false)
        << "]\n";
}

void appendMemories(std::ostringstream& out, const AICharacter& character) {
    const auto& memories = character.getMemories();
    if (memories.empty()) {
        out << "Your life so far: nothing has happened yet.\n";
        return;
    }

    std::vector<const MemoryEntry*> events;
    std::vector<const MemoryEntry*> journals;
    for (const auto& memory : memories) {
        if (memory->getKind() == "journal") {
            journals.push_back(memory.get());
        } else {
            events.push_back(memory.get());
        }
    }

    out << "Your life so far (events you lived through):\n";
    if (events.empty()) {
        out << "- none yet\n";
    } else {
        for (const auto* event : events) {
            out << "- " << event->getKind() << ": " << event->getText() << '\n';
        }
    }

    out << "Your past writings (your own voice; do NOT repeat their images, names, or rhythms):\n";
    if (journals.empty()) {
        out << "- none yet\n";
    } else {
        for (const auto* journal : journals) {
            out << "- " << journal->getText() << '\n';
        }
    }
}

std::vector<ToolDef> tickTools() {
    std::vector<ToolDef> tools;
    tools.reserve(actionSpecs().size());
    for (const auto& spec : actionSpecs()) {
        ToolDef def;
        def.name = spec.tool_name;
        def.description = spec.description;
        if (spec.has_text_param) {
            def.params.push_back({std::string{spec.text_param_name}, std::string{spec.text_param_hint}});
        }
        tools.push_back(std::move(def));
    }
    return tools;
}
} // namespace

namespace PromptBuilder {
LLMRequest forTick(const AICharacter& character, const SimulationClock& clock, const std::string& archetype) {
    std::ostringstream user;
    user << "Life fraction lived: " << clock.fractionLived() << '\n';
    appendState(user, character);
    appendMemories(user, character);

    LLMRequest request;
    request.system_prompt = tickSystemPrompt(character, archetype);
    request.user_prompt = user.str();
    request.max_tokens = 200;
    request.temperature = 0.75F;
    request.tools = tickTools();
    return request;
}

LLMRequest forLastWords(const AICharacter& character, const std::string& archetype) {
    std::ostringstream user;
    user << "Write last words in one brief first-person sentence.";
    appendState(user, character);
    appendMemories(user, character);
    return {narrativeSystemPrompt(character, archetype), user.str(), 80, 0.9F, {}};
}

LLMRequest forMemoriesFile(const AICharacter& character, const std::string& archetype) {
    std::ostringstream user;
    user << "Write a nostalgic first-person markdown retrospective titled Memories for this completed life.";
    appendState(user, character);
    appendMemories(user, character);
    return {narrativeSystemPrompt(character, archetype), user.str(), 500, 0.85F, {}};
}
} // namespace PromptBuilder
