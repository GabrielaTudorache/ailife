#ifndef AILIFE_ENUMS_H
#define AILIFE_ENUMS_H

#include <string>
#include <string_view>

enum class RelationshipType { Stranger, Friend, Rival, Family };
enum class LifeStage { Young, Adult, Elder, Dying };
enum class ActionKind { Feed, Rest, Talk, WriteJournal, SayGoodbye, InitiateChat, Reply, EndChat, Ignore };

std::string actionName(ActionKind action);
std::string lifeStageName(LifeStage stage);
ActionKind parseActionKind(std::string_view name);
LifeStage parseLifeStage(std::string_view name);
std::string relationshipTypeName(RelationshipType type);

#endif // AILIFE_ENUMS_H
