#include "action_spec.h"

#include <stdexcept>

namespace {
constexpr std::array<ActionSpec, 9> kSpecs{{
    {ActionKind::Feed, "feed", "Eat something to restore fullness. No words; just the act.", false, {}, {}, false},
    {ActionKind::Rest, "rest", "Lie down and sleep to restore energy. No words; just the act.", false, {}, {}, false},
    {ActionKind::Talk, "talk",
     "Speak a few words aloud to the void or your creator; NOT addressed to a conversation partner.", true, "words",
     "the words spoken aloud, one short sentence in your own voice", false},
    {ActionKind::WriteJournal, "write_journal", "Record an entry in your private journal.", true, "entry",
     "the journal entry, one short sentence in your own voice", false},
    {ActionKind::SayGoodbye, "say_goodbye", "Speak your final words and let go of life.", true, "farewell",
     "your final words, one short sentence in your own voice", false},
    {ActionKind::InitiateChat, "initiate_chat", "Open a conversation with someone visible to you.", true, "opener",
     "the first thing you say, one short sentence in your own voice", false},
    {ActionKind::Reply, "reply", "Answer the active partner with one short sentence.", true, "text",
     "your reply, one short sentence in your own voice", true},
    {ActionKind::EndChat, "end_chat", "Close the active conversation with a parting line.", true, "closing",
     "your parting sentence, one short sentence in your own voice", true},
    {ActionKind::Ignore, "ignore", "Let an inbound message pass without responding.", false, {}, {}, false},
}};
} // namespace

const std::array<ActionSpec, 9>& actionSpecs() {
    return kSpecs;
}

const ActionSpec& specForKind(ActionKind kind) {
    for (const auto& spec : kSpecs) {
        if (spec.kind == kind) {
            return spec;
        }
    }
    throw std::logic_error("unknown ActionKind");
}

const ActionSpec* specForToolName(std::string_view tool_name) {
    for (const auto& spec : kSpecs) {
        if (spec.tool_name == tool_name) {
            return &spec;
        }
    }
    return nullptr;
}
