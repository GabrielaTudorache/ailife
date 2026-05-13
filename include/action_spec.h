#ifndef AILIFE_ACTION_SPEC_H
#define AILIFE_ACTION_SPEC_H

#include "enums.h"

#include <array>
#include <string_view>

struct ActionSpec {
    ActionKind kind;
    std::string_view tool_name;
    std::string_view description;
    bool has_text_param;
    std::string_view text_param_name;
    std::string_view text_param_hint;
    bool has_tone_param{false};
};

const std::array<ActionSpec, 9>& actionSpecs();
const ActionSpec& specForKind(ActionKind kind);
const ActionSpec* specForToolName(std::string_view tool_name);

#endif // AILIFE_ACTION_SPEC_H
