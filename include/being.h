#ifndef AILIFE_BEING_H
#define AILIFE_BEING_H

#include "enums.h"

#include <string>

class Being {
  public:
    Being(std::string name, LifeStage life_stage);
    virtual ~Being() = default;

    int getId() const;
    const std::string& getName() const;
    LifeStage getLifeStage() const;
    virtual void tick() = 0;

  protected:
    static int next_id();
    void setName(std::string name);
    void setLifeStage(LifeStage life_stage);

  private:
    static int next_id_;

    int id_;
    std::string name_;
    LifeStage life_stage_;
};

#endif // AILIFE_BEING_H
