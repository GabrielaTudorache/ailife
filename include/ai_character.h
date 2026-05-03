#ifndef AILIFE_AI_CHARACTER_H
#define AILIFE_AI_CHARACTER_H

#include "action.h"
#include "being.h"
#include "memory_entry.h"
#include "personality.h"
#include "stat.h"

#include <iosfwd>
#include <memory>
#include <vector>

class AICharacter : public Being {
  public:
    AICharacter();
    AICharacter(std::string name, Personality personality);
    AICharacter(const AICharacter& other);
    AICharacter& operator=(const AICharacter& other);
    ~AICharacter() override;

    void tick() override;
    void apply(const Action& action);
    AICharacter& operator+=(std::unique_ptr<MemoryEntry> memory);

    const Personality& getPersonality() const;
    const Stat<int>& getHunger() const;
    const Stat<float>& getMood() const;
    const std::vector<std::unique_ptr<MemoryEntry>>& getMemories() const;
    static int getCreatedCount();

    friend std::ostream& operator<<(std::ostream& out, const AICharacter& character);
    friend std::istream& operator>>(std::istream& in, AICharacter& character);

  private:
    static int created_count_;

    Personality personality_;
    Stat<int> hunger_{0, 100};
    Stat<float> mood_{50.0F, 100.0F};
    std::vector<std::unique_ptr<MemoryEntry>> memories_;
};

AICharacter operator+(AICharacter character, std::unique_ptr<MemoryEntry> memory);

#endif // AILIFE_AI_CHARACTER_H
