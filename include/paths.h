#ifndef AILIFE_PATHS_H
#define AILIFE_PATHS_H

#include <filesystem>

namespace Paths {
std::filesystem::path memoriesDirectory();
std::filesystem::path presenceDirectory();
std::filesystem::path conversationsDirectory();
std::filesystem::path eventsDirectory();
std::filesystem::path appliedEventsDirectory();
} // namespace Paths

#endif // AILIFE_PATHS_H
