#pragma once

#include "TilesetLoader.h"
#include "JigsawMap.h"

#include <cstdint>
#include <random>
#include <unordered_map>
#include <vector>

// Manages streaming per-cell tile generation for a single tileset layer.
// Uses pre-built adjacency tables for O(1) candidate lookup per cell.
// Generates a budgeted number of tiles per update call, sorted by distance
// from a focal point (camera center), enabling non-blocking infinite scrolling.
class StreamingMapGenerator {
public:
    // Initialize with a tileset. Builds adjacency lookup tables.
    void Init(const TilesetDef& tileset);

    // Generate tiles within the given world-space rect, up to `budget` cells.
    // focalX/focalY determine generation priority (closest cells first).
    // Returns number of cells generated this call.
    int Generate(float worldLeft, float worldTop, float worldRight, float worldBottom,
                 float focalX, float focalY, int budget);

    // Access the placed tile map (for rendering).
    const JigsawMap& GetMap() const { return m_map; }
    JigsawMap& GetMap() { return m_map; }

    // Cell dimensions (derived from first tile in tileset).
    float GetCellWidth() const { return m_cellW; }
    float GetCellHeight() const { return m_cellH; }

    // Check if a cell has been generated.
    bool IsCellGenerated(int col, int row) const;

private:
    static int64_t CellKey(int col, int row) {
        return (static_cast<int64_t>(row) << 32) | static_cast<uint32_t>(col);
    }

    TilesetDef m_tileset;
    JigsawMap m_map;
    float m_cellW = 16.0f;
    float m_cellH = 16.0f;
    std::mt19937 m_rng;

    // Grid index: packed (col,row) -> tile index (-1 = gap)
    std::unordered_map<int64_t, int> m_cellGrid;

    // Pre-built adjacency: adjRight[tileIdx] = sorted list of tile indices valid to its right
    std::vector<std::vector<size_t>> m_adjRight;
    std::vector<std::vector<size_t>> m_adjDown;
};
