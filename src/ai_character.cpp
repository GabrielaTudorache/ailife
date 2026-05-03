#include "ai_character.h"

#include <iostream>
#include <utility>

int AICharacter::created_count_ = 0;

AICharacter::AICharacter() : AICharacter("Unnamed", Personality{}) {}

AICharacter::AICharacter(std::string name, Personality personality)
    : Being(std::move(name), LifeStage::Adult), personality_(std::move(personality)) {
    ++created_count_;
}

AICharacter::AICharacter(const AICharacter& other) : Being(other.getName(), other.getLifeStage()) {
    *this = other;
    ++created_count_;
}

AICharacter& AICharacter::operator=(const AICharacter& other) {
    if (this == &other) {
        return *this;
    }

    setName(other.getName());
    setLifeStage(other.getLifeStage());
    personality_ = other.personality_;
    hunger_ = other.hunger_;
    mood_ = other.mood_;
    memories_.clear();
    memories_.reserve(other.memories_.size());
    for (const auto& memory : other.memories_) {
        memories_.push_back(memory->clone());
    }

    return *this;
}

AICharacter::~AICharacter() = default;

void AICharacter::tick() {}

void AICharacter::apply(const Action& action) {
    switch (action.kind) {
    case ActionKind::Feed:
        hunger_ += 30;
        break;
    case ActionKind::Rest:
        mood_ += 5.0F;
        break;
    case ActionKind::Talk:
        mood_ += 8.0F;
        break;
    case ActionKind::WriteJournal:
        memories_.push_back(std::make_unique<JournalEntry>(action.narrative));
        break;
    case ActionKind::SayGoodbye:
        memories_.push_back(std::make_unique<LastWords>(action.narrative));
        break;
    }
}

AICharacter& AICharacter::operator+=(std::unique_ptr<MemoryEntry> memory) {
    memories_.push_back(std::move(memory));
    return *this;
}

const Personality& AICharacter::getPersonality() const {
    return personality_;
}

const Stat<int>& AICharacter::getHunger() const {
    return hunger_;
}

const Stat<float>& AICharacter::getMood() const {
    return mood_;
}

const std::vector<std::unique_ptr<MemoryEntry>>& AICharacter::getMemories() const {
    return memories_;
}

int AICharacter::getCreatedCount() {
    return created_count_;
}

std::ostream& operator<<(std::ostream& out, const AICharacter& character) {
    out << "AICharacter{id=" << character.getId() << ", name=" << character.getName()
        << ", quirk=" << character.personality_.quirk << ", hunger=" << character.hunger_.getValue() << '/'
        << character.hunger_.getMax() << ", mood=" << character.mood_.getValue() << '/' << character.mood_.getMax()
        << ", memories=[";

    for (std::size_t i = 0; i < character.memories_.size(); ++i) {
        if (i != 0) {
            out << "; ";
        }
        out << character.memories_[i]->getKind() << ':' << character.memories_[i]->getText();
    }

    return out << "]}";
}

std::istream& operator>>(std::istream& in, AICharacter& character) {
    std::string name;
    std::string quirk;
    if (in >> name >> quirk) {
        character.setName(name);
        character.personality_.quirk = quirk;
    }
    return in;
}

AICharacter operator+(AICharacter character, std::unique_ptr<MemoryEntry> memory) {
    character += std::move(memory);
    return character;
}
