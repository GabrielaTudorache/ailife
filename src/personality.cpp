#include "personality.h"

Personality personalityForArchetype(const std::string& archetype) {
    if (archetype == "curious") {
        return {90, 50, 55, 65, 35, "collects questions"};
    }
    if (archetype == "cautious") {
        return {45, 85, 25, 60, 55, "counts exits"};
    }
    if (archetype == "warm") {
        return {60, 55, 85, 90, 30, "remembers voices"};
    }
    return {75, 50, 30, 55, 80, "names the shadows"};
}
