#pragma once

#include "PlacedTile.h"
#include "SpatialIndex.h"

#include <string>
#include <vector>

// Defines the finite boundary of a JigsawMap (in pixels).
struct MapBoundary {
    float width_pixels;
    float height_pixels;
};

// Replaces the uniform-grid MapData with a free-form tile map.
// Tiles are placed at absolute pixel positions with variable dimensions.
// Backed by a SpatialIndex for efficient spatial queries.
class JigsawMap {
public:
    // --- Configuration ---
    void SetTilesetId(const std::string& id);
    const std::string& GetTilesetId() const;
    void SetBoundary(const MapBoundary& boundary);
    void ClearBoundary();
    bool HasBoundary() const;
    const MapBoundary& GetBoundary() const;

    // --- Tile management ---
    // Returns false if the tile overlaps any existing tile.
    bool AddTile(const PlacedTile& tile);
    // Removes the tile at the given position. Returns false if no tile found.
    bool RemoveTile(float x, float y);
    size_t GetTileCount() const;

    // --- Spatial queries ---
    std::vector<const PlacedTile*> QueryRect(float qx, float qy, float qw, float qh) const;
    const PlacedTile* QueryPoint(float px, float py) const;

    // --- Neighbor queries ---
    // Returns all tiles sharing a non-zero-length boundary segment with the given tile.
    std::vector<const PlacedTile*> GetEdgeNeighbors(const PlacedTile& tile) const;

    // --- Iteration ---
    const std::vector<PlacedTile>& GetAllTiles() const;

private:
    std::string m_tilesetId;
    MapBoundary m_boundary;
    bool m_hasBoundary = false;
    std::vector<PlacedTile> m_tiles;
    SpatialIndex m_index;
};
