#include "action_spec.h"

#include <stdexcept>

namespace {
constexpr std::array<ActionSpec, 5> kSpecs{{
    {ActionKind::Feed, "feed", "Eat something to restore fullness. No words; just the act.", false, {}, {}},
    {ActionKind::Rest, "rest", "Lie down and sleep to restore energy. No words; just the act.", false, {}, {}},
    {ActionKind::Talk, "talk", "Speak a few words aloud to ease loneliness.", true, "words",
     "the words spoken aloud, one short sentence in your own voice"},
    {ActionKind::WriteJournal, "write_journal", "Record an entry in your private journal.", true, "entry",
     "the journal entry, one short sentence in your own voice"},
    {ActionKind::SayGoodbye, "say_goodbye", "Speak your final words and let go of life.", true, "farewell",
     "your final words, one short sentence in your own voice"},
}};
} // namespace

const std::array<ActionSpec, 5>& actionSpecs() {
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
