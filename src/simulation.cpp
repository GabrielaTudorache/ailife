#include "simulation.h"

#include "conversation_reader.h"
#include "memory_entry.h"
#include "paths.h"
#include "prompt_builder.h"
#include "simulation_exception.h"

#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <process.h>
#define GET_PID() _getpid()
#else
#include <unistd.h>
#define GET_PID() getpid()
#endif

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
constexpr int kHardCapMessages = 15;
constexpr int kSoftNudgeMessages = 10;

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

std::vector<std::string> parseModelList(const std::string& raw) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (start <= raw.size()) {
        const std::size_t comma = raw.find(',', start);
        const std::size_t end = (comma == std::string::npos) ? raw.size() : comma;
        std::string token = raw.substr(start, end - start);
        const auto first = token.find_first_not_of(" \t\n\r");
        const auto last = token.find_last_not_of(" \t\n\r");
        if (first != std::string::npos) {
            out.emplace_back(token.substr(first, last - first + 1));
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    return out;
}

std::unique_ptr<LLMClient> makeLlm(bool mock) {
    if (mock) {
        return std::make_unique<MockLLMClient>();
    }
    const char* api_key = std::getenv("OPENROUTER_API_KEY");
    if (api_key == nullptr || std::string{api_key}.empty()) {
        throw std::runtime_error("OPENROUTER_API_KEY is required unless --mock-llm is set");
    }

    std::vector<std::string> models;

    const char* multi = std::getenv("OPENROUTER_MODELS");
    if (multi != nullptr && *multi != '\0') {
        models = parseModelList(multi);
    }

    if (models.empty()) {
        const char* model = std::getenv("OPENROUTER_MODEL");
        models.emplace_back(model == nullptr ? "deepseek/deepseek-v4-flash" : model);
    }
    return std::make_unique<RealLLMClient>(api_key, std::move(models));
}

std::string safeFilePart(std::string value) {
    std::replace_if(
        value.begin(), value.end(), [](unsigned char ch) { return !std::isalnum(ch) && ch != '-' && ch != '_'; }, '_');
    return value;
}

} // namespace

Simulation::Simulation(Cli::Config config)
    : ai_(config.name, personalityFor(config.archetype)), llm_(makeLlm(config.mock_llm)),
      strategy_(makeStrategy(config.archetype)), clock_(config.duration), my_pid_(GET_PID()),
      conversation_(my_pid_, ai_.getName(), birth_at_, logger_) {
    ai_.setLifespan(config.duration);
}

void Simulation::run() {
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
    // close any open conversations on death or stop
    if (conversation_.hasActiveConversation()) {
        const int partner_pid = conversation_.activePartnerPid().value_or(0);
        conversation_.closeActive(EndReason::PartnerGone, "");
        notifyConversationEnded(partner_pid, EndReason::PartnerGone);
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
    ai_.decayRelationships();
    onStageChanged(previous, ai_.getLifeStage());
    for (SimulationObserver* observer : observers_) {
        observer->onTick(ai_, std::chrono::duration_cast<std::chrono::seconds>(clock_.elapsed()),
                         std::chrono::duration_cast<std::chrono::seconds>(clock_.remaining()));
    }

    try {
        latest_neighbors_ = neighbors_.scan(logger_);
        latest_neighbors_.erase(std::remove_if(latest_neighbors_.begin(), latest_neighbors_.end(),
                                               [this](const PresenceSnapshot& p) { return p.pid == my_pid_; }),
                                latest_neighbors_.end());
    } catch (const std::exception& ex) {
        const std::string msg = std::string{"neighbor scan failed: "} + ex.what();
        for (SimulationObserver* observer : observers_) {
            observer->onError(msg);
        }
        latest_neighbors_.clear();
    }

    if (conversation_.hasActiveConversation()) {
        const int active_partner = conversation_.activePartnerPid().value_or(0);
        const std::string partner_name = conversation_.activePartnerName().value_or("someone");
        const auto closed = conversation_.handleTimeouts(latest_neighbors_);
        if (closed.has_value()) {
            std::string note;
            switch (*closed) {
            case EndReason::PartnerGone:
                note = partner_name + " is gone.";
                break;
            case EndReason::Timeout:
                note = partner_name + " did not answer.";
                break;
            default:
                note = "The conversation with " + partner_name + " ended.";
                break;
            }
            ai_ += std::make_unique<JournalEntry>(note);
            for (SimulationObserver* observer : observers_) {
                observer->onJournal(note);
            }
            notifyConversationEnded(active_partner, *closed);
        }
    }

    const auto updates = conversation_.pollInbox(latest_neighbors_);
    std::optional<InboxUpdate> primary_inbound;
    for (const auto& u : updates) {
        const bool relevant_active = u.is_active_partner;
        const bool relevant_opener = u.is_new_conversation && eligibleToChat();
        if ((relevant_active || relevant_opener) && !primary_inbound.has_value()) {
            primary_inbound = u;

            ai_ += std::make_unique<LetterEntry>(u.message.from_name, ai_.getName(), u.message.content);
            if (relevant_active) {
                for (SimulationObserver* observer : observers_) {
                    observer->onConversationMessage(u.partner_pid, conversation_.activeMessageCount(), u.message,
                                                    false);
                }
            }
            if (relevant_opener) {
                ai_.getOrCreateRelationship(u.partner_pid, u.partner_name);
            }
        } else {
            const std::string note = u.message.from_name + " called for me but I was elsewhere.";
            ai_ += std::make_unique<JournalEntry>(note);
            for (SimulationObserver* observer : observers_) {
                observer->onJournal(note);
            }
        }
    }

    TickContext ctx;
    ctx.visible_peers = latest_neighbors_;
    ctx.active_partner_name = conversation_.activePartnerName().value_or("");
    ctx.active_partner_pid = conversation_.activePartnerPid().value_or(0);
    ctx.can_initiate = eligibleToChat() && !conversation_.hasActiveConversation() && !primary_inbound.has_value();
    const LifeStage stage = ai_.getLifeStage();
    const bool near_end = stage == LifeStage::Elder || stage == LifeStage::Dying;
    const bool despair = ai_.getMood().getValue() < 15.0F || ai_.getLoneliness().getValue() > 90 ||
                         ai_.getHunger().getValue() < 10 || ai_.getEnergy().getValue() < 10;
    ctx.can_say_goodbye = near_end || despair;
    if (conversation_.hasActiveConversation()) {
        ctx.active_transcript = conversation_.activeTranscript();
        ctx.active_message_count = conversation_.activeMessageCount();
        ctx.soft_nudge_to_end = conversation_.activeMessageCount() >= kSoftNudgeMessages;
        const auto& rels = ai_.getRelationships();
        auto it = rels.find(ctx.active_partner_pid);
        if (it != rels.end()) {
            ctx.relationship_bond = it->second.bond;
            ctx.relationship_exchanges = it->second.messages_exchanged;
            ctx.relationship_type = it->second.type;
        }
    }
    if (primary_inbound.has_value()) {
        InboundContext ib;
        ib.partner_pid = primary_inbound->partner_pid;
        ib.partner_name = primary_inbound->partner_name;
        ib.message = primary_inbound->message;
        ib.is_active_partner = primary_inbound->is_active_partner;
        ctx.inbound = std::move(ib);

        const int partner_pid = primary_inbound->partner_pid;
        const auto& active = conversation_.activeTranscript();
        const long long past_cutoff_ms =
            active.empty() ? timestampMs(primary_inbound->message.timestamp) : timestampMs(active.front().timestamp);
        ConversationReader convo_reader;
        ctx.past_with_partner = convo_reader.messagesBefore(my_pid_, partner_pid, past_cutoff_ms, 10, logger_);

        if (!conversation_.hasActiveConversation()) {
            const auto& rels = ai_.getRelationships();
            auto it = rels.find(partner_pid);
            if (it != rels.end()) {
                ctx.relationship_bond = it->second.bond;
                ctx.relationship_exchanges = it->second.messages_exchanged;
                ctx.relationship_type = it->second.type;
            }
        }
    }

    TurnResult turn;
    try {
        turn = strategy_->decide(ai_, clock_, *llm_, ctx);
    } catch (const SimulationException& error) {
        for (SimulationObserver* observer : observers_) {
            observer->onError(error.what());
        }
        return;
    }

    if (!turn.thoughts.empty()) {
        for (SimulationObserver* observer : observers_) {
            observer->onThought(turn.thoughts);
        }
    }

    auto isBody = [](ActionKind k) {
        return k == ActionKind::Feed || k == ActionKind::Rest || k == ActionKind::Talk ||
               k == ActionKind::WriteJournal || k == ActionKind::SayGoodbye;
    };
    auto isConv = [](ActionKind k) {
        return k == ActionKind::Reply || k == ActionKind::EndChat || k == ActionKind::Ignore ||
               k == ActionKind::InitiateChat;
    };

    for (const auto& a : turn.actions) {
        if (a.kind == ActionKind::SayGoodbye) {
            for (SimulationObserver* observer : observers_) {
                observer->onDecision(a.kind);
            }
            applyBodyAction(a);
            return;
        }
    }

    std::optional<Action> body;
    std::optional<Action> conv;
    for (const auto& a : turn.actions) {
        if (isBody(a.kind) && !body.has_value()) {
            body = a;
        } else if (isConv(a.kind) && !conv.has_value()) {
            conv = a;
        }
    }

    // end_chat beats reply if both got picked
    if (conv.has_value() && conv->kind == ActionKind::Reply) {
        for (const auto& a : turn.actions) {
            if (a.kind == ActionKind::EndChat) {
                conv = a;
                break;
            }
        }
    }

    // validate action
    if (conv.has_value()) {
        const ActionKind k = conv->kind;
        bool ok = true;
        std::string reason;
        if (k == ActionKind::Reply) {
            ok = ctx.inbound.has_value() || !ctx.active_partner_name.empty();
            reason = "reply without conversation context";
        } else if (k == ActionKind::EndChat) {
            ok = conversation_.hasActiveConversation();
            reason = "end_chat without active conversation";
        } else if (k == ActionKind::InitiateChat) {
            ok = ctx.can_initiate;
            reason = "initiate_chat when not eligible";
        } else if (k == ActionKind::Ignore) {
            ok = ctx.inbound.has_value();
            reason = "ignore without inbound message";
        }
        if (!ok) {
            for (SimulationObserver* observer : observers_) {
                observer->onError("dropping action: " + reason);
            }
            conv.reset();
        }
    }

    if (conv.has_value()) {
        // conv actions take priority, remove body actions
        body.reset();

        for (SimulationObserver* observer : observers_) {
            observer->onDecision(conv->kind);
        }
        switch (conv->kind) {
        case ActionKind::Reply:
            dispatchReply(*conv, ctx);
            break;
        case ActionKind::EndChat:
            dispatchEndChat(*conv);
            break;
        case ActionKind::Ignore:
            dispatchIgnore(ctx);
            break;
        case ActionKind::InitiateChat:
            dispatchInitiateChat(*conv);
            break;
        default:
            break;
        }

        if (conversation_.hasActiveConversation() && conversation_.activeMessageCount() >= kHardCapMessages) {
            const int partner_pid = conversation_.activePartnerPid().value_or(0);
            conversation_.closeActive(EndReason::Limit, "We will speak again.");
            ai_ += std::make_unique<JournalEntry>("Our talk reached its end. We will speak again.");
            notifyConversationEnded(partner_pid, EndReason::Limit);
        }
    }

    if (body.has_value()) {
        for (SimulationObserver* observer : observers_) {
            observer->onDecision(body->kind);
        }
        applyBodyAction(*body);
    }
}

void Simulation::applyBodyAction(const Action& action) {
    ai_.apply(action);
    for (SimulationObserver* observer : observers_) {
        observer->onAction(ai_, action);
    }
    if (action.kind == ActionKind::WriteJournal && !action.narrative.empty()) {
        for (SimulationObserver* observer : observers_) {
            observer->onJournal(action.narrative);
        }
    }
    if (action.kind == ActionKind::Talk && !action.narrative.empty()) {
        for (SimulationObserver* observer : observers_) {
            observer->onSpoke(action.narrative);
        }
    }
    if (action.kind == ActionKind::SayGoodbye) {
        finished_ = true;
    }
}

void Simulation::dispatchReply(const Action& action, const TickContext& ctx) {
    if (!ctx.inbound.has_value() && ctx.active_partner_name.empty()) {
        return;
    }
    const int partner_pid = ctx.inbound.has_value() ? ctx.inbound->partner_pid : ctx.active_partner_pid;
    const std::string partner_name = ctx.inbound.has_value() ? ctx.inbound->partner_name : ctx.active_partner_name;
    const std::string text = action.narrative.empty() ? std::string{"..."} : action.narrative;
    const Tone tone = action.tone;

    bool sent = false;
    if (conversation_.hasActiveConversation()) {
        conversation_.reply(text, tone);
        sent = true;
    } else {
        sent = conversation_.initiate(partner_pid, partner_name, text, tone);
        if (sent) {
            for (SimulationObserver* observer : observers_) {
                observer->onConversationStarted(partner_pid, partner_name);
            }
        }
    }
    if (!sent) {
        return;
    }

    ai_.apply(Action{ActionKind::Reply, text, tone});
    ai_ += std::make_unique<LetterEntry>(ai_.getName(), partner_name, text);
    if (ctx.inbound.has_value()) {
        recordRelationshipExchange(partner_pid, partner_name, ctx.inbound->message.tone, true);
    }
    recordRelationshipExchange(partner_pid, partner_name, tone, false);

    Message m;
    m.from_pid = my_pid_;
    m.to_pid = partner_pid;
    m.from_name = ai_.getName();
    m.to_name = partner_name;
    m.content = text;
    m.tone = tone;
    m.timestamp = std::chrono::system_clock::now();
    for (SimulationObserver* observer : observers_) {
        observer->onConversationMessage(partner_pid, conversation_.activeMessageCount(), m, true);
    }
}

void Simulation::dispatchEndChat(const Action& action) {
    if (!conversation_.hasActiveConversation()) {
        return;
    }
    const int partner_pid = conversation_.activePartnerPid().value_or(0);
    const std::string partner_name = conversation_.activePartnerName().value_or("someone");
    const std::string closing = action.narrative;
    conversation_.closeActive(EndReason::Natural, closing);
    if (!closing.empty()) {
        ai_ += std::make_unique<LetterEntry>(ai_.getName(), partner_name, closing);
    }
    ai_ += std::make_unique<JournalEntry>("I closed the talk with " + partner_name + ".");
    recordRelationshipExchange(partner_pid, partner_name, action.tone, false);
    notifyConversationEnded(partner_pid, EndReason::Natural);
}

void Simulation::dispatchIgnore(const TickContext& ctx) {
    if (!ctx.inbound.has_value()) {
        return;
    }
    ai_ += std::make_unique<JournalEntry>("I let " + ctx.inbound->partner_name + "'s words pass.");
}

void Simulation::dispatchInitiateChat(const Action& action) {
    if (!eligibleToChat() || conversation_.hasActiveConversation()) {
        return;
    }
    const PresenceSnapshot* target = chooseInitiationTarget();
    if (target == nullptr) {
        return;
    }
    const std::string opener = action.narrative.empty() ? "Hello there." : action.narrative;
    const Tone tone = action.tone == Tone::Neutral ? Tone::Warm : action.tone;
    if (!conversation_.initiate(target->pid, target->name, opener, tone)) {
        return;
    }
    ai_ += std::make_unique<LetterEntry>(ai_.getName(), target->name, opener);
    recordRelationshipExchange(target->pid, target->name, tone, false);
    ai_.apply(Action{ActionKind::InitiateChat, opener, tone});
    for (SimulationObserver* observer : observers_) {
        observer->onConversationStarted(target->pid, target->name);
    }
    Message m;
    m.from_pid = my_pid_;
    m.to_pid = target->pid;
    m.from_name = ai_.getName();
    m.to_name = target->name;
    m.content = opener;
    m.tone = tone;
    m.timestamp = std::chrono::system_clock::now();
    for (SimulationObserver* observer : observers_) {
        observer->onConversationMessage(target->pid, conversation_.activeMessageCount(), m, true);
    }
}

void Simulation::onStageChanged(LifeStage previous, LifeStage current) {
    if (previous != current) {
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
        for (SimulationObserver* observer : observers_) {
            observer->onError(error.what());
        }
    }
    ai_.apply(Action{ActionKind::SayGoodbye, last_words});
    for (SimulationObserver* observer : observers_) {
        observer->onDeath(last_words);
    }

    std::string memories = "# Memories\n\nI lived a short life and left these words behind.\n";
    try {
        memories = llm_->complete(PromptBuilder::forMemoriesFile(ai_, strategy_->archetypeName())).content;
    } catch (const SimulationException& error) {
        for (SimulationObserver* observer : observers_) {
            observer->onError(error.what());
        }
    }
    memories += "\n\n---\n\n";
    memories += buildLifeLog();
    const auto path = writeMemoriesFile(memories);
    for (SimulationObserver* observer : observers_) {
        observer->onMemoriesWritten(path);
    }
}

std::string Simulation::buildLifeLog() const {
    std::ostringstream out;
    const auto& p = ai_.getPersonality();

    out << "## Life log\n\n"
        << "**Name:** " << ai_.getName() << "  \n"
        << "**Archetype:** " << strategy_->archetypeName() << "  \n"
        << "**Quirk:** " << p.quirk << "  \n"
        << "**Traits:** openness=" << p.openness << ", conscientiousness=" << p.conscientiousness
        << ", extraversion=" << p.extraversion << ", agreeableness=" << p.agreeableness
        << ", neuroticism=" << p.neuroticism << "  \n"
        << "**Final state:** hunger=" << ai_.getHunger().getValue() << ", energy=" << ai_.getEnergy().getValue()
        << ", mood=" << static_cast<int>(ai_.getMood().getValue()) << ", loneliness=" << ai_.getLoneliness().getValue()
        << "  \n"
        << "**Final stage:** " << lifeStageName(ai_.getLifeStage()) << "\n\n";

    const auto& rels = ai_.getRelationships();
    if (!rels.empty()) {
        out << "### Relationships at the end\n\n";
        for (const auto& [pid, rel] : rels) {
            out << "- " << rel.partner_name << " (pid " << pid << "): " << relationshipTypeName(rel.type)
                << ", bond=" << rel.bond << ", messages exchanged=" << rel.messages_exchanged << "\n";
        }
        out << '\n';
    }

    out << "### Everything that happened (in order)\n\n";
    const auto& memories = ai_.getMemories();
    if (memories.empty()) {
        out << "_Nothing was recorded._\n";
        return out.str();
    }
    int index = 1;
    for (const auto& memory : memories) {
        const std::string& kind = memory->getKind();
        out << index++ << ". ";
        if (kind == "letter") {
            const auto* letter = static_cast<const LetterEntry*>(memory.get());
            out << "**letter** _" << letter->getFrom() << " → " << letter->getTo() << ":_ " << memory->getText()
                << '\n';
        } else if (kind == "inheritance") {
            const auto* inh = static_cast<const Inheritance*>(memory.get());
            out << "**inheritance** _from " << inh->getFrom() << ":_ " << memory->getText() << '\n';
        } else if (kind == "last_words") {
            out << "**last words:** " << memory->getText() << '\n';
        } else if (kind == "journal") {
            out << "**journal:** " << memory->getText() << '\n';
        } else {
            out << "**" << kind << ":** " << memory->getText() << '\n';
        }
    }
    return out.str();
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

bool Simulation::eligibleToChat() const {
    if (ai_.getLifeStage() == LifeStage::Dying) {
        return false;
    }
    if (ai_.getEnergy().getValue() <= 20) {
        return false;
    }
    if (latest_neighbors_.empty()) {
        return false;
    }
    return true;
}

const PresenceSnapshot* Simulation::chooseInitiationTarget() const {
    const PresenceSnapshot* best = nullptr;
    float best_bond = -2.0F;
    const auto now = std::chrono::system_clock::now();
    const auto& rels = ai_.getRelationships();
    for (const auto& p : latest_neighbors_) {
        if (p.pid == my_pid_) {
            continue;
        }
        if (p.stage == LifeStage::Dying) {
            continue;
        }
        const auto age = std::chrono::duration_cast<std::chrono::seconds>(now - p.last_heartbeat);
        if (age > std::chrono::seconds{15}) {
            continue;
        }
        float bond = 0.0F;
        auto it = rels.find(p.pid);
        if (it != rels.end()) {
            bond = it->second.bond;
        }
        if (best == nullptr || bond > best_bond) {
            best = &p;
            best_bond = bond;
        }
    }
    return best;
}

void Simulation::recordRelationshipExchange(int partner_pid, const std::string& partner_name, Tone tone,
                                            bool receiver_side) {
    Relationship& rel = ai_.getOrCreateRelationship(partner_pid, partner_name);
    rel.bond = Relationship::applyToneDelta(rel.bond, tone, receiver_side);
    rel.messages_exchanged += 1;
    rel.last_interaction = std::chrono::system_clock::now();
    rel.type = Relationship::classify(rel.bond, rel.messages_exchanged);
}

void Simulation::notifyConversationEnded(int partner_pid, EndReason reason) {
    for (SimulationObserver* observer : observers_) {
        observer->onConversationEnded(partner_pid, reason);
    }
}
