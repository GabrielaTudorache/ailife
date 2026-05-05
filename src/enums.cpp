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
