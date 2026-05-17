#include "ai_character.h"

#include "memory_entry.h"

#include <chrono>
#include <iostream>
#include <utility>

int AICharacter::created_count_ = 0;

AICharacter::AICharacter() : AICharacter("Unnamed", Personality{}) {}

AICharacter::AICharacter(std::string name, Personality personality)
    : AICharacter(std::move(name), std::move(personality), MascotAppearance{}) {}

AICharacter::AICharacter(std::string name, Personality personality, MascotAppearance appearance)
    : Being(std::move(name), LifeStage::Young), personality_(std::move(personality)),
      appearance_(std::move(appearance)) {
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
    appearance_ = other.appearance_;
    hunger_ = other.hunger_;
    energy_ = other.energy_;
    mood_ = other.mood_;
    loneliness_ = other.loneliness_;
    birth_time_ = other.birth_time_;
    lifespan_ = other.lifespan_;
    memories_.clear();
    memories_.reserve(other.memories_.size());
    for (const auto& memory : other.memories_) {
        memories_.push_back(memory->clone());
    }
    relationships_ = other.relationships_;

    return *this;
}

AICharacter::~AICharacter() = default;

void AICharacter::tick() {
    applyDecay();
    recomputeStage();
}

void AICharacter::apply(const Action& action) {
    switch (action.kind) {
    case ActionKind::Feed:
        hunger_.modify(30);
        mood_.modify(2.0F);
        break;
    case ActionKind::Rest:
        energy_.modify(35);
        mood_.modify(1.0F);
        break;
    case ActionKind::Talk:
        loneliness_.modify(-8);
        mood_.modify(2.0F);
        break;
    case ActionKind::WriteJournal:
        mood_.modify(1.0F);
        memories_.push_back(std::make_unique<JournalEntry>(action.narrative));
        break;
    case ActionKind::SayGoodbye:
        memories_.push_back(std::make_unique<LastWords>(action.narrative));
        setLifeStage(LifeStage::Dying);
        break;
    case ActionKind::InitiateChat:
        loneliness_.modify(-15);
        mood_.modify(2.0F);
        break;
    case ActionKind::Reply:
        loneliness_.modify(-5);
        mood_.modify(1.0F);
        break;
    case ActionKind::EndChat:
    case ActionKind::Ignore:
        // no direct effect on stats
        break;
    }
}

void AICharacter::adjustMood(float delta) {
    mood_.modify(delta);
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

const Stat<int>& AICharacter::getEnergy() const {
    return energy_;
}

const Stat<float>& AICharacter::getMood() const {
    return mood_;
}

const Stat<int>& AICharacter::getLoneliness() const {
    return loneliness_;
}

const MascotAppearance& AICharacter::getAppearance() const {
    return appearance_;
}

const std::vector<std::unique_ptr<MemoryEntry>>& AICharacter::getMemories() const {
    return memories_;
}

void AICharacter::setLifespan(std::chrono::seconds lifespan) {
    if (lifespan <= std::chrono::seconds{0}) {
        throw std::invalid_argument("lifespan must be positive");
    }

    lifespan_ = lifespan;
    birth_time_ = std::chrono::steady_clock::now();
}

void AICharacter::recomputeStage() {
    const auto elapsed = std::chrono::steady_clock::now() - birth_time_;
    const double lived = std::chrono::duration<double>(elapsed).count();
    const double total = std::chrono::duration<double>(lifespan_).count();
    const double ratio = lived / total;

    if (ratio < 0.25) {
        setLifeStage(LifeStage::Young);
    } else if (ratio < 0.75) {
        setLifeStage(LifeStage::Adult);
    } else if (ratio < 0.95) {
        setLifeStage(LifeStage::Elder);
    } else {
        setLifeStage(LifeStage::Dying);
    }
}

bool AICharacter::isDead() const {
    return std::chrono::steady_clock::now() - birth_time_ >= lifespan_;
}

void AICharacter::applyDecay() {
    hunger_.decay();
    energy_.decay();
    mood_.decay();
    loneliness_.decay();
}

int AICharacter::getCreatedCount() {
    return created_count_;
}

const std::unordered_map<int, Relationship>& AICharacter::getRelationships() const {
    return relationships_;
}

Relationship& AICharacter::getOrCreateRelationship(int partner_pid, std::string partner_name) {
    auto it = relationships_.find(partner_pid);
    if (it == relationships_.end()) {
        Relationship newRelationship;
        newRelationship.partner_name = std::move(partner_name);
        it = relationships_.emplace(partner_pid, std::move(newRelationship)).first;
    } else if (!partner_name.empty()) {
        it->second.partner_name = std::move(partner_name);
    }
    return it->second;
}

void AICharacter::decayRelationships() {
    for (auto& [pid, rel] : relationships_) {
        rel.bond = Relationship::decay(rel.bond);
        rel.type = Relationship::classify(rel.bond, rel.messages_exchanged);
    }
}

std::ostream& operator<<(std::ostream& out, const AICharacter& character) {
    out << "AICharacter{id=" << character.getId() << ", name=" << character.getName()
        << ", quirk=" << character.personality_.quirk << ", hunger=" << character.hunger_.getValue() << '/'
        << character.hunger_.getMax() << ", energy=" << character.energy_.getValue() << '/'
        << character.energy_.getMax() << ", mood=" << character.mood_.getValue() << '/' << character.mood_.getMax()
        << ", loneliness=" << character.loneliness_.getValue() << '/' << character.loneliness_.getMax()
        << ", stage=" << static_cast<int>(character.getLifeStage())
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
