#ifndef AILIFE_COMMAND_H
#define AILIFE_COMMAND_H

class World;

// world events and creator interventions
class Command {
  public:
    virtual ~Command() = default;
    virtual void execute(World& world) const = 0;
};

#endif // AILIFE_COMMAND_H
