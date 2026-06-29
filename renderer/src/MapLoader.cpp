#include "MapLoader.h"
#include <SDL3/SDL.h>

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
