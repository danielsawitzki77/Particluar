#include "TileLabelQuery.h"

std::vector<std::string> TileLabelQuery::GetMergedLabels(
    const MapData& mapData,
    const TilesetDef& tileset,
    int row, int col)
{
    std::vector<std::string> merged;

    // Bounds check
    if (row < 0 || row >= static_cast<int>(mapData.grid.size())) return merged;
    if (col < 0 || col >= static_cast<int>(mapData.grid[row].size())) return merged;

    // Get tile definition labels
    const std::string& tileId = mapData.grid[row][col];
    auto it = tileset.idIndex.find(tileId);
    if (it != tileset.idIndex.end()) {
        const TileDef& def = tileset.tiles[it->second];
        merged = def.labels;
    }

    // Append map-level cell labels (avoiding duplicates)
    if (row < static_cast<int>(mapData.cellLabels.size()) &&
        col < static_cast<int>(mapData.cellLabels[row].size())) {
        const std::vector<std::string>& mapLabels = mapData.cellLabels[row][col];
        for (const std::string& label : mapLabels) {
            if (std::find(merged.begin(), merged.end(), label) == merged.end()) {
                merged.push_back(label);
            }
        }
    }

    return merged;
}

std::vector<std::string> TileLabelQuery::GetMergedLabels(
    const PlacedTile& placedTile,
    const TilesetDef& tileset)
{
    std::vector<std::string> merged;

    // Get tile definition labels
    auto it = tileset.idIndex.find(placedTile.tileId);
    if (it != tileset.idIndex.end()) {
        const TileDef& def = tileset.tiles[it->second];
        merged = def.labels;
    }

    // Append placement-level labels (avoiding duplicates)
    for (const std::string& label : placedTile.labels) {
        if (std::find(merged.begin(), merged.end(), label) == merged.end()) {
            merged.push_back(label);
        }
    }

    return merged;
}

bool TileLabelQuery::HasLabel(
    const MapData& mapData,
    const TilesetDef& tileset,
    int row, int col,
    const std::string& label)
{
    // Check tile definition labels first (fast path)
    if (row >= 0 && row < static_cast<int>(mapData.grid.size()) &&
        col >= 0 && col < static_cast<int>(mapData.grid[row].size())) {
        const std::string& tileId = mapData.grid[row][col];
        auto it = tileset.idIndex.find(tileId);
        if (it != tileset.idIndex.end()) {
            const TileDef& def = tileset.tiles[it->second];
            if (std::find(def.labels.begin(), def.labels.end(), label) != def.labels.end()) {
                return true;
            }
        }
    }

    // Check map-level cell labels
    if (row >= 0 && row < static_cast<int>(mapData.cellLabels.size()) &&
        col >= 0 && col < static_cast<int>(mapData.cellLabels[row].size())) {
        const std::vector<std::string>& mapLabels = mapData.cellLabels[row][col];
        if (std::find(mapLabels.begin(), mapLabels.end(), label) != mapLabels.end()) {
            return true;
        }
    }

    return false;
}

bool TileLabelQuery::HasLabel(
    const PlacedTile& placedTile,
    const TilesetDef& tileset,
    const std::string& label)
{
    // Check tile definition labels first
    auto it = tileset.idIndex.find(placedTile.tileId);
    if (it != tileset.idIndex.end()) {
        const TileDef& def = tileset.tiles[it->second];
        if (std::find(def.labels.begin(), def.labels.end(), label) != def.labels.end()) {
            return true;
        }
    }

    // Check placement-level labels
    if (std::find(placedTile.labels.begin(), placedTile.labels.end(), label) != placedTile.labels.end()) {
        return true;
    }

    return false;
}
