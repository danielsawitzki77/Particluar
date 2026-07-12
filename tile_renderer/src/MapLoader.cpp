#include "MapLoader.h"
#include <SDL3/SDL.h>
#include <cmath>

bool MapLoader::LoadMap(const std::string& filepath, MapData& out)
{
    // Parse JSON file
    picojson::value root;
    if (!JsonUtil::ParseFile(filepath, root)) {
        SDL_Log("[MapLoader] Failed to parse map file: %s", filepath.c_str());
        return false;
    }

    // Root must be an object
    if (!root.is<picojson::object>()) {
        SDL_Log("[MapLoader] Map file root is not an object: %s", filepath.c_str());
        return false;
    }

    const picojson::object& obj = root.get<picojson::object>();

    // Extract width
    int width = 0;
    if (!JsonUtil::GetInt(obj, "width", width)) {
        SDL_Log("[MapLoader] Map file missing or invalid 'width': %s", filepath.c_str());
        return false;
    }
    if (width < 1 || width > 4096) {
        SDL_Log("[MapLoader] Map file 'width' out of range (1-4096): %d in %s",
                width, filepath.c_str());
        return false;
    }

    // Extract height
    int height = 0;
    if (!JsonUtil::GetInt(obj, "height", height)) {
        SDL_Log("[MapLoader] Map file missing or invalid 'height': %s", filepath.c_str());
        return false;
    }
    if (height < 1 || height > 4096) {
        SDL_Log("[MapLoader] Map file 'height' out of range (1-4096): %d in %s",
                height, filepath.c_str());
        return false;
    }

    // Extract tileset
    std::string tileset_id;
    if (!JsonUtil::GetString(obj, "tileset", tileset_id)) {
        SDL_Log("[MapLoader] Map file missing or invalid 'tileset': %s", filepath.c_str());
        return false;
    }
    if (tileset_id.empty() || tileset_id.size() > 255) {
        SDL_Log("[MapLoader] Map file 'tileset' length out of range (1-255): %zu in %s",
                tileset_id.size(), filepath.c_str());
        return false;
    }

    // Extract grid
    const picojson::array* gridArr = nullptr;
    if (!JsonUtil::GetArray(obj, "grid", gridArr)) {
        SDL_Log("[MapLoader] Map file missing or invalid 'grid': %s", filepath.c_str());
        return false;
    }

    // Validate row count matches height
    if (static_cast<int>(gridArr->size()) != height) {
        SDL_Log("[MapLoader] Map file grid row count (%zu) does not match height (%d): %s",
                gridArr->size(), height, filepath.c_str());
        return false;
    }

    // Parse grid rows
    std::vector<std::vector<std::string>> grid;
    grid.reserve(static_cast<size_t>(height));

    for (int row = 0; row < height; ++row) {
        const picojson::value& rowVal = (*gridArr)[static_cast<size_t>(row)];
        if (!rowVal.is<picojson::array>()) {
            SDL_Log("[MapLoader] Map file grid row %d is not an array: %s",
                    row, filepath.c_str());
            return false;
        }

        const picojson::array& rowArr = rowVal.get<picojson::array>();

        // Validate column count matches width
        if (static_cast<int>(rowArr.size()) != width) {
            SDL_Log("[MapLoader] Map file grid row %d has %zu columns, expected %d: %s",
                    row, rowArr.size(), width, filepath.c_str());
            return false;
        }

        std::vector<std::string> rowData;
        rowData.reserve(static_cast<size_t>(width));

        for (int col = 0; col < width; ++col) {
            const picojson::value& cellVal = rowArr[static_cast<size_t>(col)];
            if (!cellVal.is<std::string>()) {
                SDL_Log("[MapLoader] Map file grid[%d][%d] is not a string: %s",
                        row, col, filepath.c_str());
                return false;
            }

            const std::string& cellId = cellVal.get<std::string>();
            if (cellId.empty() || cellId.size() > 128) {
                SDL_Log("[MapLoader] Map file grid[%d][%d] tile ID length out of range (1-128): %s",
                        row, col, filepath.c_str());
                return false;
            }

            rowData.push_back(cellId);
        }

        grid.push_back(std::move(rowData));
    }

    // Success - populate output
    out.width = width;
    out.height = height;
    out.tileset_id = tileset_id;
    out.grid = std::move(grid);
    out.rawJson = root;  // Preserve full JSON for round-trip fidelity

    return true;
}

std::string MapLoader::SerializeMap(const MapData& mapData) const
{
    // Start from rawJson if available (preserves unknown fields), otherwise build fresh
    picojson::object obj;

    if (mapData.rawJson.is<picojson::object>()) {
        obj = mapData.rawJson.get<picojson::object>();
    }

    // Overwrite known fields with current values
    obj["width"] = picojson::value(static_cast<double>(mapData.width));
    obj["height"] = picojson::value(static_cast<double>(mapData.height));
    obj["tileset"] = picojson::value(mapData.tileset_id);

    // Build grid array
    picojson::array gridArr;
    gridArr.reserve(mapData.grid.size());

    for (size_t row = 0; row < mapData.grid.size(); ++row) {
        picojson::array rowArr;
        rowArr.reserve(mapData.grid[row].size());

        for (size_t col = 0; col < mapData.grid[row].size(); ++col) {
            rowArr.push_back(picojson::value(mapData.grid[row][col]));
        }

        gridArr.push_back(picojson::value(rowArr));
    }

    obj["grid"] = picojson::value(gridArr);

    picojson::value root(obj);
    return JsonUtil::Serialize(root);
}

bool MapLoader::SaveMap(const std::string& filepath, const MapData& mapData)
{
    std::string json = SerializeMap(mapData);
    if (!JsonUtil::WriteFile(filepath, json)) {
        SDL_Log("[MapLoader] Failed to save map file: %s", filepath.c_str());
        return false;
    }
    return true;
}

std::vector<MapLoader::UnresolvedTile> MapLoader::ValidateAgainstTileset(
    const MapData& mapData, const TilesetDef& tileset) const
{
    std::vector<UnresolvedTile> unresolved;

    for (int row = 0; row < static_cast<int>(mapData.grid.size()); ++row) {
        for (int col = 0; col < static_cast<int>(mapData.grid[row].size()); ++col) {
            const std::string& tileId = mapData.grid[row][col];
            if (tileset.id_index.find(tileId) == tileset.id_index.end()) {
                UnresolvedTile ut;
                ut.id = tileId;
                ut.row = row;
                ut.col = col;
                unresolved.push_back(ut);
            }
        }
    }

    return unresolved;
}


// --- Jigsaw map serialization/deserialization ---

std::string MapLoader::SerializeJigsawMap(const JigsawMap& map) const
{
    picojson::object obj;

    obj["format"] = picojson::value(std::string("jigsaw"));
    obj["tileset_id"] = picojson::value(map.GetTilesetId());

    // Optional boundary
    if (map.HasBoundary()) {
        const MapBoundary& b = map.GetBoundary();
        picojson::object boundary;
        boundary["width"] = picojson::value(static_cast<double>(b.width_pixels));
        boundary["height"] = picojson::value(static_cast<double>(b.height_pixels));
        obj["boundary"] = picojson::value(boundary);
    }

    // Tiles array
    picojson::array tilesArr;
    const auto& allTiles = map.GetAllTiles();
    tilesArr.reserve(allTiles.size());

    for (const PlacedTile& tile : allTiles) {
        // Skip tiles with NaN/Inf positions or sizes during serialization
        if (!std::isfinite(tile.x) || !std::isfinite(tile.y) ||
            !std::isfinite(tile.w) || !std::isfinite(tile.h)) {
            SDL_Log("[MapLoader] Skipping tile with non-finite values during serialization: %s",
                    tile.tile_id.c_str());
            continue;
        }

        picojson::object tileObj;
        tileObj["tile_id"] = picojson::value(tile.tile_id);
        tileObj["x"] = picojson::value(static_cast<double>(tile.x));
        tileObj["y"] = picojson::value(static_cast<double>(tile.y));
        tileObj["w"] = picojson::value(static_cast<double>(tile.w));
        tileObj["h"] = picojson::value(static_cast<double>(tile.h));
        tilesArr.push_back(picojson::value(tileObj));
    }

    obj["tiles"] = picojson::value(tilesArr);

    picojson::value root(obj);
    return JsonUtil::Serialize(root);
}

bool MapLoader::LoadJigsawMap(const std::string& filepath, JigsawMap& out)
{
    // Parse JSON file
    picojson::value root;
    if (!JsonUtil::ParseFile(filepath, root)) {
        SDL_Log("[MapLoader] Failed to parse jigsaw map file: %s", filepath.c_str());
        return false;
    }

    // Root must be an object
    if (!root.is<picojson::object>()) {
        SDL_Log("[MapLoader] Jigsaw map file root is not an object: %s", filepath.c_str());
        return false;
    }

    const picojson::object& obj = root.get<picojson::object>();

    // Validate format field
    std::string format;
    if (!JsonUtil::GetString(obj, "format", format) || format != "jigsaw") {
        SDL_Log("[MapLoader] Jigsaw map file missing or invalid 'format' (expected \"jigsaw\"): %s",
                filepath.c_str());
        return false;
    }

    // Extract tileset_id
    std::string tilesetId;
    if (!JsonUtil::GetString(obj, "tileset_id", tilesetId)) {
        SDL_Log("[MapLoader] Jigsaw map file missing or invalid 'tileset_id': %s",
                filepath.c_str());
        return false;
    }

    // Build output map
    JigsawMap result;
    result.SetTilesetId(tilesetId);

    // Optional boundary
    const picojson::object* boundaryObj = nullptr;
    if (JsonUtil::GetObject(obj, "boundary", boundaryObj)) {
        double bWidth = 0.0, bHeight = 0.0;
        if (JsonUtil::GetDouble(*boundaryObj, "width", bWidth) &&
            JsonUtil::GetDouble(*boundaryObj, "height", bHeight)) {
            if (std::isfinite(static_cast<float>(bWidth)) &&
                std::isfinite(static_cast<float>(bHeight)) &&
                bWidth > 0.0 && bHeight > 0.0) {
                MapBoundary boundary;
                boundary.width_pixels = static_cast<float>(bWidth);
                boundary.height_pixels = static_cast<float>(bHeight);
                result.SetBoundary(boundary);
            } else {
                SDL_Log("[MapLoader] Jigsaw map boundary has invalid dimensions, ignoring: %s",
                        filepath.c_str());
            }
        }
    }

    // Parse tiles array
    const picojson::array* tilesArr = nullptr;
    if (!JsonUtil::GetArray(obj, "tiles", tilesArr)) {
        SDL_Log("[MapLoader] Jigsaw map file missing or invalid 'tiles' array: %s",
                filepath.c_str());
        return false;
    }

    for (size_t i = 0; i < tilesArr->size(); ++i) {
        const picojson::value& tileVal = (*tilesArr)[i];

        // Each tile must be an object
        if (!tileVal.is<picojson::object>()) {
            SDL_Log("[MapLoader] Jigsaw map tile[%zu] is not an object, skipping: %s",
                    i, filepath.c_str());
            continue;
        }

        const picojson::object& tileObj = tileVal.get<picojson::object>();

        // Validate tile_id (must be a non-empty string)
        std::string tileId;
        if (!JsonUtil::GetString(tileObj, "tile_id", tileId) || tileId.empty()) {
            SDL_Log("[MapLoader] Jigsaw map tile[%zu] missing or empty 'tile_id', skipping: %s",
                    i, filepath.c_str());
            continue;
        }

        // Validate numeric fields
        double x = 0.0, y = 0.0, w = 0.0, h = 0.0;
        if (!JsonUtil::GetDouble(tileObj, "x", x) ||
            !JsonUtil::GetDouble(tileObj, "y", y) ||
            !JsonUtil::GetDouble(tileObj, "w", w) ||
            !JsonUtil::GetDouble(tileObj, "h", h)) {
            SDL_Log("[MapLoader] Jigsaw map tile[%zu] ('%s') missing numeric fields, skipping: %s",
                    i, tileId.c_str(), filepath.c_str());
            continue;
        }

        // Check for NaN/Inf
        float fx = static_cast<float>(x);
        float fy = static_cast<float>(y);
        float fw = static_cast<float>(w);
        float fh = static_cast<float>(h);

        if (!std::isfinite(fx) || !std::isfinite(fy) ||
            !std::isfinite(fw) || !std::isfinite(fh)) {
            SDL_Log("[MapLoader] Jigsaw map tile[%zu] ('%s') has NaN/Inf position or size, skipping: %s",
                    i, tileId.c_str(), filepath.c_str());
            continue;
        }

        PlacedTile tile;
        tile.tile_id = tileId;
        tile.x = fx;
        tile.y = fy;
        tile.w = fw;
        tile.h = fh;

        // AddTile handles overlap rejection — for deserialization we add directly
        // since the data is expected to be valid (non-overlapping from a previous save).
        result.AddTile(tile);
    }

    out = std::move(result);
    return true;
}

bool MapLoader::SaveJigsawMap(const std::string& filepath, const JigsawMap& map)
{
    std::string json = SerializeJigsawMap(map);
    if (!JsonUtil::WriteFile(filepath, json)) {
        SDL_Log("[MapLoader] Failed to save jigsaw map file: %s", filepath.c_str());
        return false;
    }
    return true;
}

std::vector<MapLoader::UnresolvedJigsawTile> MapLoader::ValidateAgainstTileset(
    const JigsawMap& map, const TilesetDef& tileset) const
{
    std::vector<UnresolvedJigsawTile> unresolved;

    for (const PlacedTile& tile : map.GetAllTiles()) {
        if (tileset.id_index.find(tile.tile_id) == tileset.id_index.end()) {
            UnresolvedJigsawTile ut;
            ut.id = tile.tile_id;
            ut.x = tile.x;
            ut.y = tile.y;
            unresolved.push_back(ut);
        }
    }

    return unresolved;
}
