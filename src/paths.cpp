#include "paths.h"

#include <cstdlib>
#include <stdexcept>

namespace {
std::filesystem::path villageRoot() {
#if defined(_WIN32)
    const char* root = std::getenv("APPDATA");
#else
    const char* root = std::getenv("HOME");
#endif
    if (root == nullptr) {
        throw std::runtime_error("could not determine home directory");
    }
    return std::filesystem::path{root} / ".ailife" / "village";
}
} // namespace

namespace Paths {
std::filesystem::path memoriesDirectory() {
    auto path = villageRoot() / "memories";
    std::filesystem::create_directories(path);
    return path;
}

std::filesystem::path presenceDirectory() {
    auto path = villageRoot() / "presence";
    std::filesystem::create_directories(path);
    return path;
}

std::filesystem::path conversationsDirectory() {
    auto path = villageRoot() / "conversations";
    std::filesystem::create_directories(path);
    return path;
}
} // namespace Paths
