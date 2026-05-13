#include "stdout_observer.h"

#include "action.h"
#include "ai_character.h"

void StdoutObserver::onSimulationStarted(const AICharacter& character, const std::string& archetype) {
    logger_.event("TICK", character.getName() + " begins a " + archetype + " life.");
}

void StdoutObserver::onTick(const AICharacter& character, std::chrono::seconds /*elapsed*/,
                            std::chrono::seconds /*remaining*/) {
    logger_.event("TICK", "hunger=" + std::to_string(character.getHunger().getValue()) +
                              " energy=" + std::to_string(character.getEnergy().getValue()) +
                              " mood=" + std::to_string(static_cast<int>(character.getMood().getValue())) +
                              " loneliness=" + std::to_string(character.getLoneliness().getValue()));
}

void StdoutObserver::onThought(const std::string& text) {
    logger_.event("THINK", text);
}

void StdoutObserver::onDecision(ActionKind kind) {
    logger_.event("DECIDE", actionName(kind));
}

void StdoutObserver::onAction(const AICharacter& character, const Action& action) {
    logger_.event("ACT", character.getName() + " chose " + actionName(action.kind));
}

void StdoutObserver::onJournal(const std::string& text) {
    logger_.event("JOURNAL", text);
}

void StdoutObserver::onSpoke(const std::string& text) {
    logger_.event("SAY", text);
}

void StdoutObserver::onStageChanged(LifeStage prev, LifeStage curr) {
    logger_.event("STAGE", lifeStageName(prev) + " -> " + lifeStageName(curr));
}

void StdoutObserver::onError(const std::string& msg) {
    logger_.event("ERROR", msg);
}

void StdoutObserver::onDeath(const std::string& text) {
    logger_.event("DEATH", text);
}

void StdoutObserver::onMemoriesWritten(const std::filesystem::path& path) {
    logger_.event("DEATH", "wrote " + path.string());
}

void StdoutObserver::onConversationStarted(int /*partner_pid*/, const std::string& partner_name) {
    logger_.event("CHAT", "began talking with " + partner_name);
}

void StdoutObserver::onConversationMessage(int /*partner_pid*/, int /*sequence*/, const Message& message,
                                           bool is_outgoing) {
    if (message.is_end) {
        return;
    }
    if (is_outgoing) {
        logger_.event("SAID", message.content);
    } else {
        logger_.event("HEARD", message.from_name + ": " + message.content);
    }
}

void StdoutObserver::onConversationEnded(int /*partner_pid*/, EndReason reason) {
    logger_.event("CHAT", "talk ended (" + endReasonName(reason) + ")");
}
