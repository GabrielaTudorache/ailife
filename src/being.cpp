#include "being.h"

#include <utility>

int Being::next_id_ = 1;

Being::Being(std::string name, LifeStage life_stage)
    : id_(next_id()), name_(std::move(name)), life_stage_(life_stage) {}

int Being::getId() const {
    return id_;
}

const std::string& Being::getName() const {
    return name_;
}

LifeStage Being::getLifeStage() const {
    return life_stage_;
}

int Being::next_id() {
    return next_id_++;
}

void Being::setName(std::string name) {
    name_ = std::move(name);
}

void Being::setLifeStage(LifeStage life_stage) {
    life_stage_ = life_stage;
}
