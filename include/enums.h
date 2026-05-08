#ifndef AILIFE_ENUMS_H
#define AILIFE_ENUMS_H

#include <string>
#include <string_view>

enum class Relationship { Stranger, Friend, Rival, Family };
enum class LifeStage { Young, Adult, Elder, Dying };
enum class ActionKind { Feed, Rest, Talk, WriteJournal, SayGoodbye };

std::string actionName(ActionKind action);
std::string lifeStageName(LifeStage stage);
ActionKind parseActionKind(std::string_view name);
LifeStage parseLifeStage(std::string_view name);

#endif // AILIFE_ENUMS_H
