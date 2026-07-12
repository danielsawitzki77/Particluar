#include "StreamingMapGenerator.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <random>

void StreamingMapGenerator::Init(const TilesetDef& tileset)
{
    m_solver.Init(tileset);
    m_map = JigsawMap();
    m_map.SetTilesetId(tileset.name);
    m_cellGrid.clear();

    std::random_device rd;
    m_rng.seed(rd());
}

bool StreamingMapGenerator::IsCellGenerated(int col, int row) const
{
    return m_cellGrid.count(CellKey(col, row)) > 0;
}

int StreamingMapGenerator::Generate(float worldLeft, float worldTop,
                                    float worldRight, float worldBottom,
                                    float focalX, float focalY, int budget)
{
    if (m_solver.GetTileCount() == 0 || budget <= 0) return 0;

    float cw = m_solver.GetCellWidth();
    float ch = m_solver.GetCellHeight();
    const TilesetDef& tileset = m_solver.GetTileset();

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

        // O(1) neighbor lookup
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

        // Delegate to solver
        int chosen = m_solver.PickRandom(leftIdx, topIdx, m_rng);

        if (chosen >= 0) {
            PlacedTile pt;
            pt.tileId = tileset.tiles[chosen].id;
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
