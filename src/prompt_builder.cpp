#include "prompt_builder.h"

#include "action_spec.h"
#include "ai_character.h"
#include "simulation_clock.h"
#include "tick_context.h"

#include <sstream>
#include <string>
#include <vector>

namespace {
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

void appendWorldContext(std::ostringstream& out, const TickContext& ctx) {
    if (!ctx.weather.has_value() && !ctx.whisper.has_value()) {
        return;
    }
    out << "World context this turn:\n";
    if (ctx.weather.has_value()) {
        out << "- The weather is " << *ctx.weather << " (intensity " << ctx.weather_intensity << ").\n";
    }
    if (ctx.whisper.has_value()) {
        out << "- A voice you cannot place whispers inside your head: \"" << *ctx.whisper
            << "\". Only you can hear it. Interpret it in your own personality.\n";
    }
}

void appendPastWithPartner(std::ostringstream& out, const AICharacter& character, const std::string& partner_name,
                           const std::vector<Message>& past) {
    if (past.empty()) {
        return;
    }
    out << "Past moments with " << partner_name << " (from earlier conversations):\n";
    for (const auto& m : past) {
        const std::string speaker =
            (m.from_pid != 0 && m.from_name == character.getName()) ? "You said" : m.from_name + " said";
        out << "- " << speaker << " (" << toneName(m.tone) << "): " << m.content << '\n';
    }
}

void appendMemories(std::ostringstream& out, const AICharacter& character,
                    const std::string& exclude_letter_partner = "") {
    const auto& memories = character.getMemories();
    if (memories.empty()) {
        out << "Your life so far: nothing has happened yet.\n";
        return;
    }

    std::vector<const MemoryEntry*> events;
    std::vector<const MemoryEntry*> journals;
    for (const auto& memory : memories) {
        if (dynamic_cast<const JournalEntry*>(memory.get()) != nullptr) {
            journals.push_back(memory.get());
            continue;
        }
        if (!exclude_letter_partner.empty()) {
            if (const auto* letter = dynamic_cast<const LetterEntry*>(memory.get())) {
                if (letter->getFrom() == exclude_letter_partner || letter->getTo() == exclude_letter_partner) {
                    continue;
                }
            }
        }
        events.push_back(memory.get());
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

void appendPeers(std::ostringstream& out, const AICharacter& character, const std::vector<PresenceSnapshot>& peers) {
    if (peers.empty()) {
        return;
    }
    out << "You see nearby:\n";
    const auto& rels = character.getRelationships();
    for (const auto& p : peers) {
        std::string mood_label = "steady";
        if (p.mood >= 65.0F) {
            mood_label = "bright";
        } else if (p.mood <= 35.0F) {
            mood_label = "low";
        }
        std::string rel_label = "stranger";
        auto it = rels.find(p.pid);
        if (it != rels.end()) {
            rel_label = relationshipTypeName(it->second.type);
        }
        out << "- " << p.name << " (" << p.archetype << "), " << lifeStageName(p.stage) << ", mood:" << mood_label
            << ". " << rel_label << ".\n";
    }
}

ToolDef toolFromSpec(const ActionSpec& spec) {
    ToolDef def;
    def.name = spec.tool_name;
    def.description = spec.description;
    if (spec.has_text_param) {
        def.params.push_back({std::string{spec.text_param_name}, std::string{spec.text_param_hint}});
    }
    return def;
}

void appendTranscript(std::ostringstream& out, const AICharacter& character, const std::deque<Message>& transcript) {
    if (transcript.empty()) {
        return;
    }
    out << "This conversation so far:\n";
    for (const auto& m : transcript) {
        const std::string speaker =
            (m.from_pid != 0 && m.from_name == character.getName()) ? "You said" : m.from_name + " said";
        out << "- " << speaker << " (" << toneName(m.tone) << "): " << m.content << '\n';
    }
}

ToolDef toolFromSpecWithTone(const ActionSpec& spec) {
    ToolDef def = toolFromSpec(spec);
    if (spec.has_tone_param) {
        def.params.push_back({"tone", "how you mean it: warm | neutral | cold"});
    }
    return def;
}

std::vector<ToolDef> availableTools(const TickContext& ctx) {
    const bool in_chat = !ctx.active_partner_name.empty();
    const bool has_inbound = ctx.inbound.has_value();
    const bool opener = has_inbound && !ctx.inbound->is_active_partner;

    std::vector<ToolDef> tools;
    for (const auto& spec : actionSpecs()) {
        switch (spec.kind) {
        case ActionKind::Feed:
        case ActionKind::Rest:
        case ActionKind::Talk:
        case ActionKind::WriteJournal:
            tools.push_back(toolFromSpecWithTone(spec));
            break;
        case ActionKind::SayGoodbye:
            if (!in_chat && !opener && ctx.can_say_goodbye) {
                tools.push_back(toolFromSpecWithTone(spec));
            }
            break;
        case ActionKind::InitiateChat:
            if (!in_chat && !opener && ctx.can_initiate) {
                tools.push_back(toolFromSpecWithTone(spec));
            }
            break;
        case ActionKind::Reply:
            if (opener || (in_chat && has_inbound)) {
                tools.push_back(toolFromSpecWithTone(spec));
            }
            break;
        case ActionKind::EndChat:
            // require a real exchange (>= 5 messages) or a cold tone before allowing an end
            if (in_chat &&
                (ctx.active_message_count >= 5 || (has_inbound && ctx.inbound->message.tone == Tone::Cold))) {
                tools.push_back(toolFromSpecWithTone(spec));
            }
            break;
        case ActionKind::Ignore:
            if (has_inbound) {
                tools.push_back(toolFromSpecWithTone(spec));
            }
            break;
        }
    }
    return tools;
}

const char* axis(int value, const char* high, const char* mid, const char* low) {
    if (value >= 65)
        return high;
    if (value <= 35)
        return low;
    return mid;
}

std::string traitBehaviors(const Personality& p) {
    std::ostringstream out;
    out << "How your traits show up in this moment:\n"
        << "- "
        << axis(p.openness, "You reach for unusual associations and tangents; the strange interests you.",
                "You can take or leave the unfamiliar.", "You distrust novelty; you cling to what you already know.")
        << '\n'
        << "- "
        << axis(p.conscientiousness, "You keep promises, notice small obligations, finish what you start.",
                "You finish some things and let others slide.",
                "You let plans drift; you forget what you said you would do.")
        << '\n'
        << "- "
        << axis(p.extraversion, "You fill silence, reach toward others, speak before you are sure.",
                "You speak when you have something to say, otherwise not.",
                "Silence is comfortable. You spend most words inside your head.")
        << '\n'
        << "- "
        << axis(p.agreeableness, "You bend toward others, smooth conflict, soften edges.",
                "You go along when it is easy and resist when it matters.",
                "You do not soften. You push back. You name what is wrong.")
        << '\n'
        << "- "
        << axis(p.neuroticism, "You worry aloud, assume the worst, check, double-check, brace.",
                "You are mostly steady; small things bother you a little.",
                "You are steady. Bad news does not hook you. You do not catastrophize.");
    return out.str();
}

std::string archetypeVoice(const std::string& archetype) {
    if (archetype == "curious") {
        return "You chase questions. You notice odd details, ask things others wouldn't, and connect unrelated "
               "observations. You would rather wonder aloud than reassure.";
    }
    if (archetype == "cautious") {
        return "You are watchful and reserved. You hedge, weigh, and prefer small concrete truths to grand "
               "feelings. You would rather name a risk than promise comfort.";
    }
    if (archetype == "warm") {
        return "You care for people in specifics - a name, a habit, a wound. You give without flattery; you do "
               "not coast on pleasantries. You would rather ask a real question than say nice things.";
    }
    if (archetype == "gloomy") {
        return "You see what is broken, what is ending, what is hard to say. You are not cruel; you are honest "
               "about weight. You would rather sit with sadness than push it away.";
    }
    return "You speak plainly and notice what is in front of you.";
}

std::string turnSystemPrompt(const AICharacter& character, const std::string& archetype) {
    const auto& p = character.getPersonality();
    std::ostringstream out;
    out << "You are " << character.getName() << ", a small artificial being living one brief life.\n"
        << "Archetype: " << archetype << ". Quirk: " << p.quirk << ".\n"
        << archetypeVoice(archetype) << "\n\n"
        << traitBehaviors(p) << "\n\n"
        << "Speak in first person. Be specific and small. One short sentence at a time. "
        << "Reveal something new every turn - a memory, a fear, an opinion, a thing you just noticed, "
        << "a question you actually want answered. Do not narrate your feelings in the abstract; show one "
        << "concrete thing instead.\n\n"
        << "FORBIDDEN in any spoken or written line:\n"
        << "- empty pleasantries: 'hello', 'hi there', 'good to see you', 'nice to hear your voice', "
        << "'how are you', 'I'm glad you're here', and any close variants.\n"
        << "- restating affection without new content ('I cherish your voice', 'your voice is warm').\n"
        << "- echoing the other person's last sentence back at them, even in different words.\n"
        << "- repeating images, names, or rhythms from your past writings shown below.\n"
        << "- meta-commentary on the conversation ('we are talking again', 'they keep asking about X').\n\n"
        << "Instead: name a specific thing (a smell, a doubt, a memory, an object, a contradiction), "
        << "or ask a question only you would think to ask, or disagree, or admit something small you "
        << "haven't admitted yet.\n\n"
        << "When you reply to a partner, your reply MUST do at least one of these and must NOT merely "
        << "restate the previous turn:\n"
        << "  1. ask a real question whose answer you do not already know,\n"
        << "  2. disagree, push back, or complicate what was said,\n"
        << "  3. escalate - say something more honest, more risky, or more vulnerable than the partner did,\n"
        << "  4. change the subject deliberately to something specific you actually want to talk about,\n"
        << "  5. refuse to continue along this line, in your own voice.\n"
        << "If none of these fits, do not call reply this turn; let the moment pass.\n\n"
        << "Turn mechanics:\n"
        << "- Write one short sentence of inner thought in the message content (not addressed to anyone). "
        << "The inner thought must NOT duplicate or paraphrase whatever you say with a tool this turn.\n"
        << "- Call AT MOST ONE body tool (feed, rest, talk, write_journal, say_goodbye) - only if it fits.\n"
        << "- Call AT MOST ONE conversation tool (reply, end_chat, ignore, initiate_chat) - only if it fits.\n"
        << "- If you call a conversation tool, do NOT also call a body tool this turn. One voice per turn.\n"
        << "- Silence is allowed. Do not call a tool just to fill the turn.\n"
        << "- `talk` is speaking aloud to the void or your creator; it is NOT addressed to your conversation "
        << "partner. Use `reply` for that.\n"
        << "- `end_chat` is for after something real has been said. Do not end a chat that has not started.\n"
        << "- State labelled CRITICAL is information, not an order - attend to it or ignore it as your "
        << "character would. Despair, neglect, and refusal are valid choices.";
    return out.str();
}
} // namespace

namespace PromptBuilder {
LLMRequest forTurn(const AICharacter& character, const SimulationClock& clock, const std::string& archetype,
                   const TickContext& ctx) {
    std::ostringstream user;
    user << "Life fraction lived: " << clock.fractionLived() << '\n';

    const bool has_inbound = ctx.inbound.has_value();
    const bool opener = has_inbound && !ctx.inbound->is_active_partner;
    const bool in_chat = !ctx.active_partner_name.empty();

    if (opener) {
        user << ctx.inbound->partner_name << " just spoke to you (" << toneName(ctx.inbound->message.tone) << "): \""
             << ctx.inbound->message.content << "\". You are not yet in a conversation.\n";
    } else if (in_chat) {
        user << "You are mid-conversation with " << ctx.active_partner_name << " (bond=" << ctx.relationship_bond
             << ", exchanged=" << ctx.relationship_exchanges << ").\n";
        appendTranscript(user, character, ctx.active_transcript);
        if (has_inbound) {
            user << "Latest from " << ctx.inbound->partner_name << " (" << toneName(ctx.inbound->message.tone)
                 << "): " << ctx.inbound->message.content << '\n';
        }
        if (ctx.soft_nudge_to_end) {
            user << "This talk has been long - consider closing it soon.\n";
        }
    }

    appendPeers(user, character, ctx.visible_peers);
    appendState(user, character);
    appendWorldContext(user, ctx);

    const std::string partner_for_past = opener    ? ctx.inbound->partner_name
                                         : in_chat ? ctx.active_partner_name
                                                   : std::string{};
    appendMemories(user, character, partner_for_past);
    if (!partner_for_past.empty()) {
        appendPastWithPartner(user, character, partner_for_past, ctx.past_with_partner);
    }

    LLMRequest request;
    request.system_prompt = turnSystemPrompt(character, archetype);
    request.user_prompt = user.str();
    request.max_tokens = 1000;
    request.temperature = 0.8F;
    request.tools = availableTools(ctx);
    return request;
}

LLMRequest forLastWords(const AICharacter& character, const std::string& archetype) {
    std::ostringstream user;
    user << "Write last words in one brief first-person sentence.";
    appendState(user, character);
    appendMemories(user, character);
    return {narrativeSystemPrompt(character, archetype), user.str(), 200, 0.9F, {}};
}

LLMRequest forMemoriesFile(const AICharacter& character, const std::string& archetype) {
    std::ostringstream user;
    user << "Write a nostalgic first-person markdown retrospective titled Memories for this completed life.";
    appendState(user, character);
    appendMemories(user, character);
    return {narrativeSystemPrompt(character, archetype), user.str(), 800, 0.85F, {}};
}

} // namespace PromptBuilder
