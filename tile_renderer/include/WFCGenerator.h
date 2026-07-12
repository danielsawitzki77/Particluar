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
    float targetWidth;             // target area width (pixels)
    float targetHeight;            // target area height (pixels)
    float originX, originY;      // top-left of target area
    unsigned int seed;              // 0 = non-deterministic
    const TilesetDef* tileset;      // must not be null
    float layerScale;              // default 1.0 (applied during generation)

    JigsawWFCParams()
        : targetWidth(0), targetHeight(0)
        , originX(0), originY(0)
        , seed(0), tileset(nullptr)
        , layerScale(1.0f) {}
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

    // Target area center for flood-fill ordering (set before generation)
    float m_centerX = 0.0f;
    float m_centerY = 0.0f;

    // Gap queue comparator: prioritize gaps closer to center (flood-fill outward).
    // std::priority_queue is max-heap, so we invert: further = "less" priority.
    struct GapCompare {
        float cx, cy;
        GapCompare() : cx(0), cy(0) {}
        GapCompare(float centerX, float centerY) : cx(centerX), cy(centerY) {}
        bool operator()(const Gap& a, const Gap& b) const {
            // Distance from gap center to target center
            float ax = a.x + a.w * 0.5f - cx;
            float ay = a.y + a.h * 0.5f - cy;
            float bx = b.x + b.w * 0.5f - cx;
            float by = b.y + b.h * 0.5f - cy;
            float distA = ax * ax + ay * ay;
            float distB = bx * bx + by * by;
            return distA > distB;  // smaller distance = higher priority
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
