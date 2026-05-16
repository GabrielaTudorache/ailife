#ifndef AILIFE_SPAWN_PROCESS_H
#define AILIFE_SPAWN_PROCESS_H

#include <string>

#include "mascot_renderer.h"
#include "personality.h"

struct SpawnOptions {
    std::string name;
    std::string archetype{"curious"};
    double lifespan_minutes{8.0};
    MascotAppearance appearance;
    bool custom_personality{false};
    Personality personality;
};

namespace SpawnProcess {
long spawnHeadlessAI(const SpawnOptions& options, bool mock_llm);

void terminate(long pid);
} // namespace SpawnProcess

#endif // AILIFE_SPAWN_PROCESS_H
