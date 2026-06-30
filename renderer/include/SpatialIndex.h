#pragma once

#include "PlacedTile.h"

#include <cstdint>
#include <cmath>
#include <vector>
#include <unordered_map>
#include <algorithm>

// Grid cell coordinate in the spatial hash.
struct CellCoord {
    int cx, cy;
};

// Grid-based spatial hash for fast axis-aligned rectangle queries.
// The world is divided into square cells of configurable size (default 256px).
// Each cell stores indices into an external tile vector.
class SpatialIndex {
public:
    void SetCellSize(int size);
    int GetCellSize() const;

    void Clear();
    void Insert(size_t tileIndex, const PlacedTile& tile);
    void Remove(size_t tileIndex, const PlacedTile& tile);
    void Rebuild(const std::vector<PlacedTile>& tiles);

    // Returns indices of tiles whose bounding rects intersect the query rect.
    std::vector<size_t> Query(float qx, float qy, float qw, float qh) const;

private:
    int m_cellSize = 256;
    std::unordered_map<int64_t, std::vector<size_t>> m_cells;

    // Stored bounding rects for intersection verification during Query.
    struct TileRect {
        float x, y, w, h;
    };
    std::unordered_map<size_t, TileRect> m_tileRects;

    int64_t PackKey(int cx, int cy) const;
    CellCoord ToCellCoord(float px, float py) const;
};
