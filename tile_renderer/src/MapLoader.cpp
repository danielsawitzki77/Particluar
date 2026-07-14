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
    std::string tilesetIdStr;
    if (!JsonUtil::GetString(obj, "tileset", tilesetIdStr)) {
        SDL_Log("[MapLoader] Map file missing or invalid 'tileset': %s", filepath.c_str());
        return false;
    }
    if (tilesetIdStr.empty() || tilesetIdStr.size() > 255) {
        SDL_Log("[MapLoader] Map file 'tileset' length out of range (1-255): %zu in %s",
                tilesetIdStr.size(), filepath.c_str());
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
    out.tilesetId = tilesetIdStr;
    out.grid = std::move(grid);
    out.rawJson = root;  // Preserve full JSON for round-trip fidelity

    // Parse optional "cell_labels" — a 2D array matching grid dimensions.
    // cell_labels[row][col] is either null/missing (no extra labels) or an array of strings.
    out.cellLabels.clear();
    const picojson::array* cellLabelsArr = nullptr;
    if (JsonUtil::GetArray(obj, "cell_labels", cellLabelsArr)) {
        out.cellLabels.reserve(static_cast<size_t>(height));
        for (int row = 0; row < height; ++row) {
            std::vector<std::vector<std::string>> rowLabels;
            rowLabels.resize(static_cast<size_t>(width));

            if (row < static_cast<int>(cellLabelsArr->size())) {
                const picojson::value& rowVal = (*cellLabelsArr)[static_cast<size_t>(row)];
                if (rowVal.is<picojson::array>()) {
                    const picojson::array& rowArr = rowVal.get<picojson::array>();
                    for (int col = 0; col < width && col < static_cast<int>(rowArr.size()); ++col) {
                        const picojson::value& cellVal = rowArr[static_cast<size_t>(col)];
                        if (cellVal.is<picojson::array>()) {
                            const picojson::array& lblArr = cellVal.get<picojson::array>();
                            for (size_t li = 0; li < lblArr.size(); ++li) {
                                if (lblArr[li].is<std::string>()) {
                                    const std::string& lbl = lblArr[li].get<std::string>();
                                    if (!lbl.empty()) {
                                        rowLabels[static_cast<size_t>(col)].push_back(lbl);
                                    }
                                }
                            }
                        }
                        // null or non-array entries = no labels for that cell (already empty)
                    }
                }
            }

            out.cellLabels.push_back(std::move(rowLabels));
        }
    }

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
    obj["tileset"] = picojson::value(mapData.tilesetId);

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

    // Serialize cell_labels if any cell has labels
    bool hasAnyLabels = false;
    for (size_t row = 0; row < mapData.cellLabels.size() && !hasAnyLabels; ++row) {
        for (size_t col = 0; col < mapData.cellLabels[row].size() && !hasAnyLabels; ++col) {
            if (!mapData.cellLabels[row][col].empty()) {
                hasAnyLabels = true;
            }
        }
    }

    if (hasAnyLabels) {
        picojson::array cellLabelsArr;
        cellLabelsArr.reserve(mapData.cellLabels.size());

        for (size_t row = 0; row < mapData.cellLabels.size(); ++row) {
            picojson::array rowArr;
            rowArr.reserve(mapData.cellLabels[row].size());

            for (size_t col = 0; col < mapData.cellLabels[row].size(); ++col) {
                const std::vector<std::string>& labels = mapData.cellLabels[row][col];
                if (labels.empty()) {
                    rowArr.push_back(picojson::value());  // null = no labels
                } else {
                    picojson::array lblArr;
                    lblArr.reserve(labels.size());
                    for (const std::string& lbl : labels) {
                        lblArr.push_back(picojson::value(lbl));
                    }
                    rowArr.push_back(picojson::value(lblArr));
                }
            }

            cellLabelsArr.push_back(picojson::value(rowArr));
        }

        obj["cell_labels"] = picojson::value(cellLabelsArr);
    } else {
        // Remove cell_labels key if previously present but now empty
        obj.erase("cell_labels");
    }

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
            if (tileset.idIndex.find(tileId) == tileset.idIndex.end()) {
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
        boundary["width"] = picojson::value(static_cast<double>(b.widthPixels));
        boundary["height"] = picojson::value(static_cast<double>(b.heightPixels));
        obj["boundary"] = picojson::value(boundary);
    }

    // Optional map-wide labels
    const auto& mapLabels = map.GetMapLabels();
    if (!mapLabels.empty()) {
        picojson::array labelsArr;
        labelsArr.reserve(mapLabels.size());
        for (const std::string& lbl : mapLabels) {
            labelsArr.push_back(picojson::value(lbl));
        }
        obj["map_labels"] = picojson::value(labelsArr);
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
                    tile.tileId.c_str());
            continue;
        }

        picojson::object tileObj;
        tileObj["tile_id"] = picojson::value(tile.tileId);
        tileObj["x"] = picojson::value(static_cast<double>(tile.x));
        tileObj["y"] = picojson::value(static_cast<double>(tile.y));
        tileObj["w"] = picojson::value(static_cast<double>(tile.w));
        tileObj["h"] = picojson::value(static_cast<double>(tile.h));

        // Serialize per-placement labels if any exist
        if (!tile.labels.empty()) {
            picojson::array lblArr;
            lblArr.reserve(tile.labels.size());
            for (const std::string& lbl : tile.labels) {
                lblArr.push_back(picojson::value(lbl));
            }
            tileObj["labels"] = picojson::value(lblArr);
        }

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
                boundary.widthPixels = static_cast<float>(bWidth);
                boundary.heightPixels = static_cast<float>(bHeight);
                result.SetBoundary(boundary);
            } else {
                SDL_Log("[MapLoader] Jigsaw map boundary has invalid dimensions, ignoring: %s",
                        filepath.c_str());
            }
        }
    }

    // Optional map-wide labels
    const picojson::array* mapLabelsArr = nullptr;
    if (JsonUtil::GetArray(obj, "map_labels", mapLabelsArr)) {
        std::vector<std::string> mapLabels;
        for (size_t i = 0; i < mapLabelsArr->size(); ++i) {
            if ((*mapLabelsArr)[i].is<std::string>()) {
                const std::string& lbl = (*mapLabelsArr)[i].get<std::string>();
                if (!lbl.empty()) {
                    mapLabels.push_back(lbl);
                }
            }
        }
        result.SetMapLabels(mapLabels);
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

        // Validate tileId (must be a non-empty string)
        std::string tileId;
        if (!JsonUtil::GetString(tileObj, "tile_id", tileId) || tileId.empty()) {
            SDL_Log("[MapLoader] Jigsaw map tile[%zu] missing or empty 'tileId', skipping: %s",
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
        tile.tileId = tileId;
        tile.x = fx;
        tile.y = fy;
        tile.w = fw;
        tile.h = fh;

        // Parse optional per-placement labels
        const picojson::array* labelsArr = nullptr;
        if (JsonUtil::GetArray(tileObj, "labels", labelsArr)) {
            for (size_t li = 0; li < labelsArr->size(); ++li) {
                if ((*labelsArr)[li].is<std::string>()) {
                    const std::string& lbl = (*labelsArr)[li].get<std::string>();
                    if (!lbl.empty()) {
                        tile.labels.push_back(lbl);
                    }
                }
            }
        }

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
        if (tileset.idIndex.find(tile.tileId) == tileset.idIndex.end()) {
            UnresolvedJigsawTile ut;
            ut.id = tile.tileId;
            ut.x = tile.x;
            ut.y = tile.y;
            unresolved.push_back(ut);
        }
    }

    return unresolved;
}
