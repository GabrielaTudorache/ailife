#ifndef AILIFE_PERSONALITY_H
#define AILIFE_PERSONALITY_H

#include <string>

struct Personality {
    int openness{50};
    int conscientiousness{50};
    int extraversion{50};
    int agreeableness{50};
    int neuroticism{50};
    std::string quirk{"quiet"};
};

#endif // AILIFE_PERSONALITY_H
