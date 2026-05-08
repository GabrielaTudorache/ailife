#include "enums.h"

std::string actionName(ActionKind action) {
    switch (action) {
    case ActionKind::Feed:
        return "FEED";
    case ActionKind::Rest:
        return "REST";
    case ActionKind::Talk:
        return "TALK";
    case ActionKind::WriteJournal:
        return "WRITE_JOURNAL";
    case ActionKind::SayGoodbye:
        return "SAY_GOODBYE";
    }
    return "WRITE_JOURNAL";
}

std::string lifeStageName(LifeStage stage) {
    switch (stage) {
    case LifeStage::Young:
        return "Young";
    case LifeStage::Adult:
        return "Adult";
    case LifeStage::Elder:
        return "Elder";
    case LifeStage::Dying:
        return "Dying";
    }
    return "Unknown";
}

ActionKind parseActionKind(std::string_view name) {
    if (name == "FEED") {
        return ActionKind::Feed;
    }
    if (name == "REST") {
        return ActionKind::Rest;
    }
    if (name == "TALK") {
        return ActionKind::Talk;
    }
    if (name == "SAY_GOODBYE") {
        return ActionKind::SayGoodbye;
    }
    return ActionKind::WriteJournal;
}

LifeStage parseLifeStage(std::string_view name) {
    if (name == "Adult") {
        return LifeStage::Adult;
    }
    if (name == "Elder") {
        return LifeStage::Elder;
    }
    if (name == "Dying") {
        return LifeStage::Dying;
    }
    return LifeStage::Young;
}
