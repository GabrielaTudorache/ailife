#ifndef AILIFE_AI_CHARACTER_H
#define AILIFE_AI_CHARACTER_H

#include "action.h"
#include "being.h"
#include "mascot_renderer.h"
#include "memory_entry.h"
#include "personality.h"
#include "relationship.h"
#include "stat.h"

#include <chrono>
#include <iosfwd>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class AICharacter : public Being {
  public:
    AICharacter();
    AICharacter(std::string name, Personality personality);
    AICharacter(std::string name, Personality personality, MascotAppearance appearance);
    AICharacter(const AICharacter& other);
    AICharacter& operator=(const AICharacter& other);
    ~AICharacter() override;

    void tick() override;
    void apply(const Action& action);
    AICharacter& operator+=(std::unique_ptr<MemoryEntry> memory);

    const Personality& getPersonality() const;
    const Stat<int>& getHunger() const;
    const Stat<int>& getEnergy() const;
    const Stat<float>& getMood() const;
    const Stat<int>& getLoneliness() const;
    const MascotAppearance& getAppearance() const;
    const std::vector<std::unique_ptr<MemoryEntry>>& getMemories() const;
    void setLifespan(std::chrono::seconds lifespan);
    void recomputeStage();
    bool isDead() const;
    void applyDecay();
    void adjustMood(float delta);
    static int getCreatedCount();

    const std::unordered_map<int, Relationship>& getRelationships() const;
    Relationship& getOrCreateRelationship(int partner_pid, std::string partner_name);
    void decayRelationships();
    int closeFriendCount() const;

    friend std::ostream& operator<<(std::ostream& out, const AICharacter& character);
    friend std::istream& operator>>(std::istream& in, AICharacter& character);

  private:
    static int created_count_;

    Personality personality_;
    MascotAppearance appearance_;
    Stat<int> hunger_{80, 0, 100, 4};
    Stat<int> energy_{80, 0, 100, 3};
    Stat<float> mood_{50.0F, 0.0F, 100.0F, 1.5F};
    Stat<int> loneliness_{25, 0, 100, -3};
    std::chrono::steady_clock::time_point birth_time_{std::chrono::steady_clock::now()};
    std::chrono::seconds lifespan_{std::chrono::minutes{8}};
    std::vector<std::unique_ptr<MemoryEntry>> memories_;
    std::unordered_map<int, Relationship> relationships_;
};

AICharacter operator+(AICharacter character, std::unique_ptr<MemoryEntry> memory);

#endif // AILIFE_AI_CHARACTER_H
