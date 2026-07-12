#include "StreamingMapGenerator.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <random>

void StreamingMapGenerator::Init(const TilesetDef& tileset)
{
    m_tileset = tileset;
    m_map = JigsawMap();
    m_map.SetTilesetId(tileset.name);
    m_cellGrid.clear();

    // Determine cell size from first tile
    float sheetScale = tileset.sheetScale;
    if (!tileset.tiles.empty()) {
        const TileDef& ft = tileset.tiles[0];
        m_cellW = static_cast<float>(ft.sourceRect.w) * sheetScale * ft.scale;
        m_cellH = static_cast<float>(ft.sourceRect.h) * sheetScale * ft.scale;
    }
    if (m_cellW < 1.0f) m_cellW = 1.0f;
    if (m_cellH < 1.0f) m_cellH = 1.0f;

    // Seed RNG
    std::random_device rd;
    m_rng.seed(rd());

    // Pre-build adjacency lookup tables (index-based, O(n^2) once at init)
    int nt = static_cast<int>(tileset.tiles.size());
    m_adjRight.clear();
    m_adjDown.clear();
    m_adjRight.resize(nt);
    m_adjDown.resize(nt);

    for (int a = 0; a < nt; ++a) {
        const TileDef& tileA = tileset.tiles[a];
        for (int b = 0; b < nt; ++b) {
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

    SDL_Log("[StreamingMapGenerator] Initialized '%s': %d tiles, cell %.0fx%.0f",
            tileset.name.c_str(), nt, m_cellW, m_cellH);
}

bool StreamingMapGenerator::IsCellGenerated(int col, int row) const
{
    return m_cellGrid.count(CellKey(col, row)) > 0;
}

int StreamingMapGenerator::Generate(float worldLeft, float worldTop,
                                    float worldRight, float worldBottom,
                                    float focalX, float focalY, int budget)
{
    if (m_tileset.tiles.empty() || budget <= 0) return 0;

    int numTiles = static_cast<int>(m_tileset.tiles.size());
    float cw = m_cellW;
    float ch = m_cellH;

    int colMin = static_cast<int>(std::floor(worldLeft / cw));
    int colMax = static_cast<int>(std::ceil(worldRight / cw));
    int rowMin = static_cast<int>(std::floor(worldTop / ch));
    int rowMax = static_cast<int>(std::ceil(worldBottom / ch));

    // Collect ungenerated cells, sorted by distance from focal point
    struct CellEntry { int col, row; float dist; };
    std::vector<CellEntry> pending;

    for (int row = rowMin; row <= rowMax; ++row) {
        for (int col = colMin; col <= colMax; ++col) {
            if (m_cellGrid.count(CellKey(col, row)) > 0) continue;
            float cx = (static_cast<float>(col) + 0.5f) * cw;
            float cy = (static_cast<float>(row) + 0.5f) * ch;
            float dx = cx - focalX;
            float dy = cy - focalY;
            pending.push_back({ col, row, dx * dx + dy * dy });
        }
    }

    std::sort(pending.begin(), pending.end(),
        [](const CellEntry& a, const CellEntry& b) { return a.dist < b.dist; });

    int generated = 0;

    for (const CellEntry& cell : pending) {
        if (generated >= budget) break;
        ++generated;

        // O(1) neighbor lookup from grid index
        int leftIdx = -1;
        {
            auto it = m_cellGrid.find(CellKey(cell.col - 1, cell.row));
            if (it != m_cellGrid.end()) leftIdx = it->second;
        }

        int topIdx = -1;
        {
            auto it = m_cellGrid.find(CellKey(cell.col, cell.row - 1));
            if (it != m_cellGrid.end()) topIdx = it->second;
        }

        // Intersect adjacency sets for candidate tiles
        std::vector<int> candidates;
        if (leftIdx >= 0 && topIdx >= 0) {
            const auto& rightSet = m_adjRight[leftIdx];
            const auto& downSet = m_adjDown[topIdx];
            for (size_t ri = 0, di = 0; ri < rightSet.size() && di < downSet.size(); ) {
                if (rightSet[ri] == downSet[di]) {
                    candidates.push_back(static_cast<int>(rightSet[ri]));
                    ++ri; ++di;
                } else if (rightSet[ri] < downSet[di]) {
                    ++ri;
                } else {
                    ++di;
                }
            }
        } else if (leftIdx >= 0) {
            for (size_t idx : m_adjRight[leftIdx])
                candidates.push_back(static_cast<int>(idx));
        } else if (topIdx >= 0) {
            for (size_t idx : m_adjDown[topIdx])
                candidates.push_back(static_cast<int>(idx));
        } else {
            candidates.resize(numTiles);
            for (int t = 0; t < numTiles; ++t) candidates[t] = t;
        }

        if (!candidates.empty()) {
            std::uniform_int_distribution<int> dist(0, static_cast<int>(candidates.size()) - 1);
            int chosen = candidates[dist(m_rng)];
            PlacedTile pt;
            pt.tileId = m_tileset.tiles[chosen].id;
            pt.x = static_cast<float>(cell.col) * cw;
            pt.y = static_cast<float>(cell.row) * ch;
            pt.w = cw;
            pt.h = ch;
            m_map.AddTile(pt);
            m_cellGrid[CellKey(cell.col, cell.row)] = chosen;
        } else {
            m_cellGrid[CellKey(cell.col, cell.row)] = -1; // gap
        }
    }

    return generated;
}
