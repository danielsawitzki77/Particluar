#include "SpatialIndex.h"

// --- Configuration ---

void SpatialIndex::SetCellSize(int size) {
    if (size > 0) {
        m_cellSize = size;
    }
}

int SpatialIndex::GetCellSize() const {
    return m_cellSize;
}

// --- Cell coordinate computation (sub-task 2.2) ---

CellCoord SpatialIndex::ToCellCoord(float px, float py) const {
    CellCoord c;
    c.cx = static_cast<int>(std::floor(px / static_cast<float>(m_cellSize)));
    c.cy = static_cast<int>(std::floor(py / static_cast<float>(m_cellSize)));
    return c;
}

int64_t SpatialIndex::PackKey(int cx, int cy) const {
    return (static_cast<int64_t>(cx) << 32) | (static_cast<int64_t>(cy) & 0xFFFFFFFF);
}

// --- Insert (sub-task 2.3) ---

void SpatialIndex::Insert(size_t tileIndex, const PlacedTile& tile) {
    // Store the bounding rect for later intersection verification.
    TileRect rect;
    rect.x = tile.x;
    rect.y = tile.y;
    rect.w = tile.w;
    rect.h = tile.h;
    m_tileRects[tileIndex] = rect;

    // Compute the range of cells this tile overlaps.
    CellCoord minCell = ToCellCoord(tile.x, tile.y);
    CellCoord maxCell = ToCellCoord(tile.x + tile.w - 0.001f, tile.y + tile.h - 0.001f);

    for (int cy = minCell.cy; cy <= maxCell.cy; ++cy) {
        for (int cx = minCell.cx; cx <= maxCell.cx; ++cx) {
            int64_t key = PackKey(cx, cy);
            m_cells[key].push_back(tileIndex);
        }
    }
}

// --- Remove (sub-task 2.4) ---

void SpatialIndex::Remove(size_t tileIndex, const PlacedTile& tile) {
    CellCoord minCell = ToCellCoord(tile.x, tile.y);
    CellCoord maxCell = ToCellCoord(tile.x + tile.w - 0.001f, tile.y + tile.h - 0.001f);

    for (int cy = minCell.cy; cy <= maxCell.cy; ++cy) {
        for (int cx = minCell.cx; cx <= maxCell.cx; ++cx) {
            int64_t key = PackKey(cx, cy);
            auto it = m_cells.find(key);
            if (it != m_cells.end()) {
                std::vector<size_t>& vec = it->second;
                vec.erase(std::remove(vec.begin(), vec.end(), tileIndex), vec.end());
                if (vec.empty()) {
                    m_cells.erase(it);
                }
            }
        }
    }

    m_tileRects.erase(tileIndex);
}

// --- Query (sub-task 2.5) ---

std::vector<size_t> SpatialIndex::Query(float qx, float qy, float qw, float qh) const {
    // Compute all cells that intersect the query rect.
    CellCoord minCell = ToCellCoord(qx, qy);
    CellCoord maxCell = ToCellCoord(qx + qw - 0.001f, qy + qh - 0.001f);

    // Collect candidate tile indices from overlapping cells.
    std::vector<size_t> candidates;
    for (int cy = minCell.cy; cy <= maxCell.cy; ++cy) {
        for (int cx = minCell.cx; cx <= maxCell.cx; ++cx) {
            int64_t key = PackKey(cx, cy);
            auto it = m_cells.find(key);
            if (it != m_cells.end()) {
                const std::vector<size_t>& vec = it->second;
                candidates.insert(candidates.end(), vec.begin(), vec.end());
            }
        }
    }

    // Deduplicate: sort then unique.
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());

    // Verify actual bounding rect intersection for each candidate.
    std::vector<size_t> results;
    results.reserve(candidates.size());
    for (size_t idx : candidates) {
        auto rectIt = m_tileRects.find(idx);
        if (rectIt == m_tileRects.end()) continue;
        const TileRect& t = rectIt->second;
        // Two rects do NOT intersect if one is entirely to the left/right/above/below.
        bool noIntersect = (t.x + t.w <= qx) || (t.x >= qx + qw) ||
                           (t.y + t.h <= qy) || (t.y >= qy + qh);
        if (!noIntersect) {
            results.push_back(idx);
        }
    }

    return results;
}

// --- Clear ---

void SpatialIndex::Clear() {
    m_cells.clear();
    m_tileRects.clear();
}

// --- Rebuild (sub-task 2.6) ---

void SpatialIndex::Rebuild(const std::vector<PlacedTile>& tiles) {
    Clear();
    for (size_t i = 0; i < tiles.size(); ++i) {
        Insert(i, tiles[i]);
    }
}
