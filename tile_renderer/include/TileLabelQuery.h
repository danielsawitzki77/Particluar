#pragma once

#include "TilesetLoader.h"
#include "MapLoader.h"
#include "JigsawMap.h"

#include <string>
#include <vector>
#include <algorithm>

// Provides merged label queries for placed tiles.
// Merges tile-definition labels with map-level (per-placement) labels.
class TileLabelQuery {
public:
    // Returns the merged set of labels for a grid-based map cell.
    // Combines TileDef::labels with MapData::cellLabels[row][col].
    // Duplicates are removed. Order is definition labels first, then map labels.
    static std::vector<std::string> GetMergedLabels(
        const MapData& mapData,
        const TilesetDef& tileset,
        int row, int col);

    // Returns the merged set of labels for a jigsaw map tile.
    // Combines TileDef::labels with PlacedTile::labels.
    static std::vector<std::string> GetMergedLabels(
        const PlacedTile& placedTile,
        const TilesetDef& tileset);

    // Check if a grid cell has a specific label (in the merged set).
    static bool HasLabel(
        const MapData& mapData,
        const TilesetDef& tileset,
        int row, int col,
        const std::string& label);

    // Check if a placed tile has a specific label (in the merged set).
    static bool HasLabel(
        const PlacedTile& placedTile,
        const TilesetDef& tileset,
        const std::string& label);
};
