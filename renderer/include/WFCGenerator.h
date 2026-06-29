#pragma once

#include "MapLoader.h"
#include "TilesetLoader.h"
#include <vector>
#include <random>
#include <utility>
#include <queue>

enum class WFCStatus {
    Success,
    Contradiction,
    InvalidInput
};

struct WFCParams {
    int width;                  // 1-1024
    int height;                 // 1-1024
    unsigned int seed;          // 0 = non-deterministic (use random_device)
    const TilesetDef* tileset;  // must not be null
};

struct WFCResult {
    WFCStatus status;
    MapData map;                // valid only if status == Success
};

class WFCGenerator {
public:
    WFCResult Generate(const WFCParams& params);

private:
    struct Cell {
        std::vector<bool> possibilities;  // one bit per tile
        int entropy;                       // count of true bits
        bool collapsed;
    };

    // Select cell with minimum entropy (excluding already-collapsed).
    // Returns {row, col} or {-1,-1} if all collapsed.
    // Breaks ties randomly using rng.
    std::pair<int, int> SelectMinEntropy(
        const std::vector<std::vector<Cell>>& grid, std::mt19937& rng) const;

    // Collapse a cell to one random remaining tile.
    void Collapse(Cell& cell, std::mt19937& rng);

    // Propagate constraints from a modified cell outward via BFS.
    // Returns false if a contradiction is found (any cell has 0 possibilities).
    bool Propagate(std::vector<std::vector<Cell>>& grid,
                   int startRow, int startCol,
                   const TilesetDef& tileset);
};
