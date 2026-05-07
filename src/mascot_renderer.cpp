#include "mascot_renderer.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr int kCanvasRows = 15;
constexpr int kCanvasCols = 17;

using CellGrid = std::vector<std::vector<std::string>>;

// split string into codepoints, so that we can overlay them correctly
// (e.g. emoji that are multiple bytes long)
std::vector<std::string> splitCodepoints(const std::string& line) {
    std::vector<std::string> cells;
    for (std::size_t i = 0; i < line.size();) {
        const auto byte = static_cast<unsigned char>(line[i]);
        std::size_t len = 1;
        if ((byte & 0x80U) == 0U) {
            len = 1;
        } else if ((byte & 0xE0U) == 0xC0U) {
            len = 2;
        } else if ((byte & 0xF0U) == 0xE0U) {
            len = 3;
        } else if ((byte & 0xF8U) == 0xF0U) {
            len = 4;
        }
        if (i + len > line.size()) {
            break;
        }
        cells.push_back(line.substr(i, len));
        i += len;
    }
    return cells;
}

// allow running from project root or from build directory during dev
std::filesystem::path resolveAssetPath(const std::string& name) {
    const std::filesystem::path candidates[] = {
        std::filesystem::path{"assets"} / name,
        std::filesystem::path{".."} / "assets" / name,
    };
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    return candidates[0];
}

CellGrid loadGrid(const std::string& asset) {
    CellGrid grid(kCanvasRows, std::vector<std::string>(kCanvasCols, " "));
    std::ifstream in{resolveAssetPath(asset)};
    if (!in) {
        return grid;
    }
    std::string line;
    int row = 0;
    while (row < kCanvasRows && std::getline(in, line)) {
        const auto cells = splitCodepoints(line);
        const int width = std::min<int>(kCanvasCols, static_cast<int>(cells.size()));
        for (int col = 0; col < width; ++col) {
            grid[row][col] = cells[col];
        }
        ++row;
    }
    return grid;
}

// paint non-empty cells from top onto base
void overlay(CellGrid& base, const CellGrid& top) {
    for (int r = 0; r < kCanvasRows; ++r) {
        for (int c = 0; c < kCanvasCols; ++c) {
            const auto& cell = top[r][c];
            if (!cell.empty() && cell != " ") {
                base[r][c] = cell;
            }
        }
    }
}

std::vector<std::string> joinRows(const CellGrid& grid) {
    std::vector<std::string> rows;
    rows.reserve(kCanvasRows);
    for (const auto& cells : grid) {
        std::string row;
        for (const auto& cell : cells) {
            row += cell;
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

struct LayerSet {
    std::string eyes;
    std::string mouth;
};

LayerSet layersFor(MascotState state) {
    switch (state) {
    case MascotState::Happy:
        return {"eyes_happy.txt", "mouth_happy.txt"};
    case MascotState::Neutral:
        return {"eyes_neutral_1.txt", "mouth_neutral_1.txt"};
    case MascotState::Sad:
        return {"eyes_sad.txt", "mouth_sad.txt"};
    case MascotState::Sleeping:
        return {"eyes_sleeping.txt", "mouth_neutral_1.txt"};
    case MascotState::Dying:
        return {"eyes_dying.txt", "mouth_dying.txt"};
    }
    return {"eyes_neutral_1.txt", "mouth_neutral_1.txt"};
}

std::vector<std::string> renderFrame(MascotState state) {
    auto grid = loadGrid("base.txt");
    const auto layers = layersFor(state);
    overlay(grid, loadGrid(layers.eyes));
    overlay(grid, loadGrid(layers.mouth));
    return joinRows(grid);
}

} // namespace

std::vector<std::string> MascotRenderer::frameFor(MascotState state) {
    static std::mutex cache_mutex;
    static std::unordered_map<int, std::vector<std::string>> cache;

    std::scoped_lock lock{cache_mutex};
    const int key = static_cast<int>(state);
    if (auto it = cache.find(key); it != cache.end()) {
        return it->second;
    }
    auto frame = renderFrame(state);
    cache.emplace(key, frame);
    return frame;
}

MascotState pickMascotState(LifeStage stage, float mood, ActionKind last_action) {
    if (stage == LifeStage::Dying) {
        return MascotState::Dying;
    }
    if (last_action == ActionKind::Rest) {
        return MascotState::Sleeping;
    }
    if (mood >= 65.0F) {
        return MascotState::Happy;
    }
    if (mood <= 35.0F) {
        return MascotState::Sad;
    }
    return MascotState::Neutral;
}
