#include "JigsawMap.h"

#include <cmath>
#include <algorithm>

// Tolerance for floating-point edge comparisons.
static const float kEdgeTolerance = 0.001f;

// --- Configuration ---

void JigsawMap::SetTilesetId(const std::string& id) {
    m_tilesetId = id;
}

const std::string& JigsawMap::GetTilesetId() const {
    return m_tilesetId;
}

void JigsawMap::SetBoundary(const MapBoundary& boundary) {
    if (boundary.width_pixels > 0.0f && boundary.height_pixels > 0.0f) {
        m_boundary = boundary;
        m_hasBoundary = true;
    }
}

void JigsawMap::ClearBoundary() {
    m_hasBoundary = false;
}

bool JigsawMap::HasBoundary() const {
    return m_hasBoundary;
}

const MapBoundary& JigsawMap::GetBoundary() const {
    return m_boundary;
}

// --- Tile management ---

bool JigsawMap::AddTile(const PlacedTile& tile) {
    // Query the spatial index for any existing tile overlapping the candidate rect.
    std::vector<size_t> hits = m_index.Query(tile.x, tile.y, tile.w, tile.h);
    if (!hits.empty()) {
        return false; // overlap detected
    }

    m_tiles.push_back(tile);
    m_index.Insert(m_tiles.size() - 1, tile);
    return true;
}

bool JigsawMap::RemoveTile(float x, float y) {
    const PlacedTile* found = QueryPoint(x, y);
    if (!found) {
        return false;
    }

    // Determine index of the found tile in the vector.
    size_t idx = static_cast<size_t>(found - m_tiles.data());
    m_tiles.erase(m_tiles.begin() + static_cast<std::ptrdiff_t>(idx));

    // Rebuild the spatial index after removal (simplest correct approach).
    m_index.Rebuild(m_tiles);
    return true;
}

size_t JigsawMap::GetTileCount() const {
    return m_tiles.size();
}

// --- Spatial queries ---

std::vector<const PlacedTile*> JigsawMap::QueryRect(float qx, float qy, float qw, float qh) const {
    std::vector<size_t> indices = m_index.Query(qx, qy, qw, qh);
    std::vector<const PlacedTile*> results;
    results.reserve(indices.size());
    for (size_t i : indices) {
        results.push_back(&m_tiles[i]);
    }
    return results;
}

const PlacedTile* JigsawMap::QueryPoint(float px, float py) const {
    // A point query is a rect query with a tiny size.
    std::vector<size_t> indices = m_index.Query(px, py, 0.001f, 0.001f);
    for (size_t i : indices) {
        const PlacedTile& t = m_tiles[i];
        if (px >= t.x && px < t.x + t.w && py >= t.y && py < t.y + t.h) {
            return &t;
        }
    }
    return nullptr;
}

// --- Neighbor queries ---

std::vector<const PlacedTile*> JigsawMap::GetEdgeNeighbors(const PlacedTile& tile) const {
    // Query a slightly expanded rect around the tile to catch edge-adjacent tiles.
    float expand = kEdgeTolerance * 2.0f;
    float qx = tile.x - expand;
    float qy = tile.y - expand;
    float qw = tile.w + expand * 2.0f;
    float qh = tile.h + expand * 2.0f;

    std::vector<size_t> indices = m_index.Query(qx, qy, qw, qh);

    std::vector<const PlacedTile*> neighbors;
    for (size_t i : indices) {
        const PlacedTile& candidate = m_tiles[i];

        // Skip self (pointer comparison on same vector, or value comparison).
        if (&candidate == &tile) {
            continue;
        }
        // Also skip if the candidate is at the exact same position/size (for externally
        // provided tile references that aren't literally the same object).
        if (candidate.x == tile.x && candidate.y == tile.y &&
            candidate.w == tile.w && candidate.h == tile.h &&
            candidate.tile_id == tile.tile_id) {
            continue;
        }

        // Check right adjacency: candidate.x == tile.x + tile.w (within tolerance)
        // AND vertical overlap > 0
        float overlapY = std::min(tile.y + tile.h, candidate.y + candidate.h) -
                         std::max(tile.y, candidate.y);
        float overlapX = std::min(tile.x + tile.w, candidate.x + candidate.w) -
                         std::max(tile.x, candidate.x);

        bool rightAdj = std::fabs(candidate.x - (tile.x + tile.w)) < kEdgeTolerance &&
                        overlapY > 0.0f;
        bool leftAdj = std::fabs(tile.x - (candidate.x + candidate.w)) < kEdgeTolerance &&
                       overlapY > 0.0f;
        bool bottomAdj = std::fabs(candidate.y - (tile.y + tile.h)) < kEdgeTolerance &&
                         overlapX > 0.0f;
        bool topAdj = std::fabs(tile.y - (candidate.y + candidate.h)) < kEdgeTolerance &&
                      overlapX > 0.0f;

        if (rightAdj || leftAdj || bottomAdj || topAdj) {
            neighbors.push_back(&candidate);
        }
    }

    return neighbors;
}

// --- Iteration ---

const std::vector<PlacedTile>& JigsawMap::GetAllTiles() const {
    return m_tiles;
}
