#pragma once

#include "MapLoader.h"
#include "TilesetLoader.h"
#include "JigsawMap.h"
#include "EffectiveSize.h"
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

// --- Jigsaw WFC types ---

struct Gap {
    float x, y, w, h;  // unfilled rectangular region
};

struct JigsawWFCParams {
    float target_width;             // target area width (pixels)
    float target_height;            // target area height (pixels)
    float origin_x, origin_y;      // top-left of target area
    unsigned int seed;              // 0 = non-deterministic
    const TilesetDef* tileset;      // must not be null
    float layer_scale;              // default 1.0 (applied during generation)

    JigsawWFCParams()
        : target_width(0), target_height(0)
        , origin_x(0), origin_y(0)
        , seed(0), tileset(nullptr)
        , layer_scale(1.0f) {}
};

struct JigsawWFCResult {
    WFCStatus status;
    JigsawMap map;                  // valid only if status == Success
};

class WFCGenerator {
public:
    // Legacy grid-based generation (kept for compatibility)
    WFCResult Generate(const WFCParams& params);

    // New jigsaw generation
    JigsawWFCResult GenerateJigsaw(const JigsawWFCParams& params);

private:
    // --- Legacy grid WFC helpers ---
    struct Cell {
        std::vector<bool> possibilities;  // one bit per tile
        int entropy;                       // count of true bits
        bool collapsed;
    };

    std::pair<int, int> SelectMinEntropy(
        const std::vector<std::vector<Cell>>& grid, std::mt19937& rng) const;

    void Collapse(Cell& cell, std::mt19937& rng);

    bool Propagate(std::vector<std::vector<Cell>>& grid,
                   int startRow, int startCol,
                   const TilesetDef& tileset);

    // --- Jigsaw WFC helpers ---

    // Gap queue comparator: y first (smallest), then x (smallest).
    // std::priority_queue is max-heap, so we invert comparison.
    struct GapCompare {
        bool operator()(const Gap& a, const Gap& b) const {
            if (a.y != b.y) return a.y > b.y;  // smaller y = higher priority
            return a.x > b.x;                   // smaller x = higher priority
        }
    };
    using GapQueue = std::priority_queue<Gap, std::vector<Gap>, GapCompare>;

    // Find tiles that fit a given gap considering adjacency constraints.
    // Returns indices into tileset.tiles.
    std::vector<size_t> GetCandidates(
        const Gap& gap,
        const TilesetDef& tileset,
        const JigsawMap& placed,
        float sheetScale, float layerScale) const;

    // Validate adjacency between a candidate tile and its already-placed neighbors.
    bool ValidateAdjacency(
        const PlacedTile& candidate,
        const TileDef& candidateDef,
        const JigsawMap& placed,
        const TilesetDef& tileset) const;

    // After placing a tile, split the remaining gap into sub-gaps.
    std::vector<Gap> SplitGap(const Gap& original, const PlacedTile& placed) const;
};
