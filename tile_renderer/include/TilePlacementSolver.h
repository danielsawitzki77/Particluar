#pragma once

#include "TilesetLoader.h"
#include <cstdint>
#include <random>
#include <unordered_map>
#include <vector>

// Core tile placement algorithm.
// Builds adjacency lookup tables from a tileset and resolves valid candidates
// for a cell given its neighbors. Used by both StreamingMapGenerator (realtime)
// and MapGenerator (offline CLI).
//
// This class is the single source of truth for adjacency-constrained tile selection.
class TilePlacementSolver {
public:
    // Initialize from a tileset. Builds O(n^2) adjacency tables.
    void Init(const TilesetDef& tileset);

    // Resolve candidates for a cell given left and top neighbor tile indices.
    // Returns list of valid tile indices. Pass -1 for absent neighbors.
    const std::vector<int>& Resolve(int leftTileIdx, int topTileIdx);

    // Pick a random tile from candidates. Returns -1 if no valid tile.
    int PickRandom(int leftTileIdx, int topTileIdx, std::mt19937& rng);

    // Cell size derived from tileset.
    float GetCellWidth() const { return m_cellW; }
    float GetCellHeight() const { return m_cellH; }
    int GetTileCount() const { return m_numTiles; }
    const TilesetDef& GetTileset() const { return m_tileset; }

    // Direct access to adjacency tables (for advanced usage).
    const std::vector<std::vector<size_t>>& GetAdjRight() const { return m_adjRight; }
    const std::vector<std::vector<size_t>>& GetAdjDown() const { return m_adjDown; }

private:
    TilesetDef m_tileset;
    int m_numTiles = 0;
    float m_cellW = 16.0f;
    float m_cellH = 16.0f;

    // adjRight[a] = sorted list of tile indices valid to the right of tile a
    std::vector<std::vector<size_t>> m_adjRight;
    // adjDown[a] = sorted list of tile indices valid below tile a
    std::vector<std::vector<size_t>> m_adjDown;

    // Cached resolve result (avoids repeated allocation)
    std::vector<int> m_resolveCache;
};
