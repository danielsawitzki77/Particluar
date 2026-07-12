#pragma once

#include "TilePlacementSolver.h"
#include "JigsawMap.h"

#include <cstdint>
#include <random>
#include <unordered_map>
#include <vector>

// Streaming tile generator for infinite scrollable maps.
// Uses TilePlacementSolver for adjacency-constrained cell resolution.
// Generates a budgeted number of tiles per frame, sorted by distance
// from a focal point (camera center), enabling non-blocking scrolling.
class StreamingMapGenerator {
public:
    // Initialize with a tileset. Delegates to TilePlacementSolver for adjacency.
    void Init(const TilesetDef& tileset);

    // Generate tiles within the given world-space rect, up to `budget` cells.
    // focalX/focalY determine generation priority (closest cells first).
    // Returns number of cells generated this call.
    int Generate(float worldLeft, float worldTop, float worldRight, float worldBottom,
                 float focalX, float focalY, int budget);

    // Access the placed tile map (for rendering).
    const JigsawMap& GetMap() const { return m_map; }
    JigsawMap& GetMap() { return m_map; }

    float GetCellWidth() const { return m_solver.GetCellWidth(); }
    float GetCellHeight() const { return m_solver.GetCellHeight(); }

    bool IsCellGenerated(int col, int row) const;

private:
    static int64_t CellKey(int col, int row) {
        return (static_cast<int64_t>(row) << 32) | static_cast<uint32_t>(col);
    }

    TilePlacementSolver m_solver;
    JigsawMap m_map;
    std::mt19937 m_rng;

    // Grid index: packed (col,row) -> tile index (-1 = gap)
    std::unordered_map<int64_t, int> m_cellGrid;
};
