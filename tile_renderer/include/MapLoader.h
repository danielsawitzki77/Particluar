#pragma once

#include "JigsawMap.h"
#include "JsonUtil.h"
#include "TilesetLoader.h"
#include <string>
#include <vector>

struct MapData {
    int width;                              // 1–4096
    int height;                             // 1–4096
    std::string tileset_id;                 // 1–255 chars
    std::vector<std::vector<std::string>> grid; // row-major, grid[row][col] = tile ID
    picojson::value rawJson;                // preserved for round-trip fidelity
};

class MapLoader {
public:
    // Parse a map file from disk.
    bool LoadMap(const std::string& filepath, MapData& out);

    // Serialize a MapData to JSON string (2-space pretty-print).
    std::string SerializeMap(const MapData& mapData) const;

    // Write map to disk.
    bool SaveMap(const std::string& filepath, const MapData& mapData);

    // Validate tile IDs in grid against tileset. Returns list of unresolved IDs+positions.
    struct UnresolvedTile { std::string id; int row; int col; };
    std::vector<UnresolvedTile> ValidateAgainstTileset(
        const MapData& mapData, const TilesetDef& tileset) const;

    // --- Jigsaw map format ---

    // Load a jigsaw map from a JSON file. Skips malformed tile records.
    bool LoadJigsawMap(const std::string& filepath, JigsawMap& out);

    // Serialize a JigsawMap to a JSON string (2-space pretty-print).
    std::string SerializeJigsawMap(const JigsawMap& map) const;

    // Write a jigsaw map to disk.
    bool SaveJigsawMap(const std::string& filepath, const JigsawMap& map);

    // Validate jigsaw tile IDs against tileset. Returns unresolved tiles.
    struct UnresolvedJigsawTile { std::string id; float x; float y; };
    std::vector<UnresolvedJigsawTile> ValidateAgainstTileset(
        const JigsawMap& map, const TilesetDef& tileset) const;
};
