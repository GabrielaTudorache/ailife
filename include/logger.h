#ifndef AILIFE_LOGGER_H
#define AILIFE_LOGGER_H

#include <string>
#include <string_view>

class Logger {
  public:
    Logger();
    void event(std::string_view tag, const std::string& text) const;

  private:
    bool use_color_{false};
};

#endif // AILIFE_LOGGER_H
