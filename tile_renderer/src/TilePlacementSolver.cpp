#include "TilePlacementSolver.h"
#include <SDL3/SDL.h>

void TilePlacementSolver::Init(const TilesetDef& tileset)
{
    m_tileset = tileset;
    m_numTiles = static_cast<int>(tileset.tiles.size());

    // Derive cell size from first tile
    float sheetScale = tileset.sheetScale;
    if (!tileset.tiles.empty()) {
        const TileDef& ft = tileset.tiles[0];
        m_cellW = static_cast<float>(ft.sourceRect.w) * sheetScale * ft.scale;
        m_cellH = static_cast<float>(ft.sourceRect.h) * sheetScale * ft.scale;
    }
    if (m_cellW < 1.0f) m_cellW = 1.0f;
    if (m_cellH < 1.0f) m_cellH = 1.0f;

    // Build adjacency tables: O(n^2) at init, O(1) lookups at runtime
    m_adjRight.clear();
    m_adjDown.clear();
    m_adjRight.resize(m_numTiles);
    m_adjDown.resize(m_numTiles);

    for (int a = 0; a < m_numTiles; ++a) {
        const TileDef& tileA = tileset.tiles[a];
        for (int b = 0; b < m_numTiles; ++b) {
            const TileDef& tileB = tileset.tiles[b];

            // Can B be to the RIGHT of A?
            bool rightOk = true;
            if (!tileA.adjacency.right.empty()) {
                bool found = false;
                for (const auto& id : tileA.adjacency.right) { if (id == tileB.id) { found = true; break; } }
                if (!found) rightOk = false;
            }
            if (rightOk && !tileB.adjacency.left.empty()) {
                bool found = false;
                for (const auto& id : tileB.adjacency.left) { if (id == tileA.id) { found = true; break; } }
                if (!found) rightOk = false;
            }
            if (rightOk) m_adjRight[a].push_back(b);

            // Can B be BELOW A?
            bool downOk = true;
            if (!tileA.adjacency.down.empty()) {
                bool found = false;
                for (const auto& id : tileA.adjacency.down) { if (id == tileB.id) { found = true; break; } }
                if (!found) downOk = false;
            }
            if (downOk && !tileB.adjacency.up.empty()) {
                bool found = false;
                for (const auto& id : tileB.adjacency.up) { if (id == tileA.id) { found = true; break; } }
                if (!found) downOk = false;
            }
            if (downOk) m_adjDown[a].push_back(b);
        }
    }

    SDL_Log("[TilePlacementSolver] Initialized: %d tiles, cell %.0fx%.0f",
            m_numTiles, m_cellW, m_cellH);
}

const std::vector<int>& TilePlacementSolver::Resolve(int leftTileIdx, int topTileIdx)
{
    m_resolveCache.clear();

    if (leftTileIdx >= 0 && topTileIdx >= 0) {
        // Intersect adjRight[left] and adjDown[top] (both sorted by index)
        const auto& rightSet = m_adjRight[leftTileIdx];
        const auto& downSet = m_adjDown[topTileIdx];
        for (size_t ri = 0, di = 0; ri < rightSet.size() && di < downSet.size(); ) {
            if (rightSet[ri] == downSet[di]) {
                m_resolveCache.push_back(static_cast<int>(rightSet[ri]));
                ++ri; ++di;
            } else if (rightSet[ri] < downSet[di]) {
                ++ri;
            } else {
                ++di;
            }
        }
    } else if (leftTileIdx >= 0) {
        for (size_t idx : m_adjRight[leftTileIdx])
            m_resolveCache.push_back(static_cast<int>(idx));
    } else if (topTileIdx >= 0) {
        for (size_t idx : m_adjDown[topTileIdx])
            m_resolveCache.push_back(static_cast<int>(idx));
    } else {
        // No constraints
        m_resolveCache.resize(m_numTiles);
        for (int t = 0; t < m_numTiles; ++t) m_resolveCache[t] = t;
    }

    return m_resolveCache;
}

int TilePlacementSolver::PickRandom(int leftTileIdx, int topTileIdx, std::mt19937& rng)
{
    const std::vector<int>& candidates = Resolve(leftTileIdx, topTileIdx);
    if (candidates.empty()) return -1;

    // Compute total weight from chance values (0 means tile is excluded)
    int totalWeight = 0;
    for (int idx : candidates) {
        int w = m_tileset.tiles[idx].chance;
        if (w > 0) totalWeight += w;
    }

    // If all candidates have zero chance, fall back to uniform selection
    if (totalWeight <= 0) {
        std::uniform_int_distribution<int> dist(0, static_cast<int>(candidates.size()) - 1);
        return candidates[dist(rng)];
    }

    // Weighted random selection
    std::uniform_int_distribution<int> dist(1, totalWeight);
    int roll = dist(rng);
    int accum = 0;
    for (int idx : candidates) {
        int w = m_tileset.tiles[idx].chance;
        if (w <= 0) continue;
        accum += w;
        if (roll <= accum) return idx;
    }

    // Fallback (should not reach here)
    return candidates.back();
}
