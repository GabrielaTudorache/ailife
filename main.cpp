#include "action.h"
#include "ai_character.h"
#include "memory_entry.h"
#include "personality.h"

#include <iostream>
#include <memory>

int main() {
    Personality ada_personality{80, 70, 40, 65, 35, "collects shells"};
    AICharacter ada{"Ada", ada_personality};

    Personality teo_personality{45, 60, 75, 55, 50, "hums at night"};
    AICharacter teo{"Teo", teo_personality};

    ada += std::make_unique<JournalEntry>("Saw rain for the first time");
    teo += std::make_unique<LetterEntry>("Ada", "Teo", "I found a blue stone today. It was beautiful.");
    teo = teo + std::make_unique<Inheritance>("Ada", "Teo, take care of the shells for me.");

    ada.apply(Action{ActionKind::Feed, "", ""});
    teo.apply(Action{ActionKind::WriteJournal, "The night hums back when I hum.", ""});

    AICharacter test;
    std::cin >> test;

    std::cout << ada << '\n';
    std::cout << teo << '\n';
    std::cout << test << '\n';
    std::cout << "Characters created: " << AICharacter::getCreatedCount() << '\n';

    return 0;
}
