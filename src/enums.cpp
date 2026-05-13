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
    case ActionKind::InitiateChat:
        return "INITIATE_CHAT";
    case ActionKind::Reply:
        return "REPLY";
    case ActionKind::EndChat:
        return "END_CHAT";
    case ActionKind::Ignore:
        return "IGNORE";
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
    if (name == "INITIATE_CHAT") {
        return ActionKind::InitiateChat;
    }
    if (name == "REPLY") {
        return ActionKind::Reply;
    }
    if (name == "END_CHAT") {
        return ActionKind::EndChat;
    }
    if (name == "IGNORE") {
        return ActionKind::Ignore;
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

std::string relationshipTypeName(RelationshipType type) {
    switch (type) {
    case RelationshipType::Stranger:
        return "stranger";
    case RelationshipType::Friend:
        return "friend";
    case RelationshipType::Rival:
        return "rival";
    case RelationshipType::Family:
        return "family";
    }
    return "stranger";
}
