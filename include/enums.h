#ifndef AILIFE_ENUMS_H
#define AILIFE_ENUMS_H

#include <string>

enum class Relationship { Stranger, Friend, Rival, Family };
enum class LifeStage { Young, Adult, Elder, Dying };
enum class ActionKind { Feed, Rest, Talk, WriteJournal, SayGoodbye };

std::string actionName(ActionKind action);
std::string lifeStageName(LifeStage stage);

#endif // AILIFE_ENUMS_H
