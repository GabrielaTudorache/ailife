#include "paths.h"

#include <cstdlib>
#include <stdexcept>

namespace Paths {
std::filesystem::path memoriesDirectory() {
#if defined(_WIN32)
    const char* root = std::getenv("APPDATA");
#else
    const char* root = std::getenv("HOME");
#endif
    if (root == nullptr) {
        throw std::runtime_error("could not determine home directory");
    }

    auto path = std::filesystem::path{root} / ".ailife" / "village" / "memories";
    std::filesystem::create_directories(path);
    return path;
}
} // namespace Paths
