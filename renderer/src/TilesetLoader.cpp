#include "TilesetLoader.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <set>

// Helper: extract a string array from a picojson array value
static bool ExtractStringArray(const picojson::array& arr, std::vector<std::string>& out)
{
    out.clear();
    out.reserve(arr.size());
    for (size_t i = 0; i < arr.size(); ++i) {
        if (!arr[i].is<std::string>()) {
            return false;
        }
        out.push_back(arr[i].get<std::string>());
    }
    return true;
}

// Helper: find the first .png and .json file pair in a folder
static bool FindTilesetFiles(const std::string& folderPath,
                             std::string& outPngPath,
                             std::string& outJsonPath,
                             std::string& outName)
{
    // Derive the tileset name from the folder path (last path component)
    std::string folder = folderPath;
    // Remove trailing slashes
    while (!folder.empty() && (folder.back() == '/' || folder.back() == '\\')) {
        folder.pop_back();
    }
    // Find last separator
    size_t sep = folder.find_last_of("/\\");
    if (sep != std::string::npos) {
        outName = folder.substr(sep + 1);
    } else {
        outName = folder;
    }

    // Expect <name>.png and <name>.json in the folder
    outPngPath = folderPath + "/" + outName + ".png";
    outJsonPath = folderPath + "/" + outName + ".json";

    return true;
}

bool TilesetLoader::ParseSidecarJson(const std::string& jsonPath, int texW, int texH,
                                     std::vector<TileDef>& outTiles, picojson::value& rawJson)
{
    // Parse JSON file
    if (!JsonUtil::ParseFile(jsonPath, rawJson)) {
        SDL_Log("[TilesetLoader] Failed to parse sidecar JSON: %s", jsonPath.c_str());
        return false;
    }

    // Root must be an object
    if (!rawJson.is<picojson::object>()) {
        SDL_Log("[TilesetLoader] Sidecar JSON root is not an object: %s", jsonPath.c_str());
        return false;
    }

    const picojson::object& root = rawJson.get<picojson::object>();

    // Extract "tiles" array
    const picojson::array* tilesArr = nullptr;
    if (!JsonUtil::GetArray(root, "tiles", tilesArr)) {
        SDL_Log("[TilesetLoader] Sidecar JSON missing 'tiles' array: %s", jsonPath.c_str());
        return false;
    }

    outTiles.clear();
    outTiles.reserve(tilesArr->size());

    std::set<std::string> seenIds;

    for (size_t i = 0; i < tilesArr->size(); ++i) {
        const picojson::value& tileVal = (*tilesArr)[i];

        if (!tileVal.is<picojson::object>()) {
            SDL_Log("[TilesetLoader] Tile entry %zu is not an object, skipping.", i);
            continue;
        }

        const picojson::object& tileObj = tileVal.get<picojson::object>();

        // Extract id
        std::string id;
        if (!JsonUtil::GetString(tileObj, "id", id) || id.empty()) {
            SDL_Log("[TilesetLoader] Tile entry %zu: missing or empty 'id', skipping.", i);
            continue;
        }

        // Validate id length
        if (id.size() > 64) {
            SDL_Log("[TilesetLoader] Tile entry %zu: id '%s' exceeds 64 chars, skipping.",
                    i, id.c_str());
            continue;
        }

        // Check for duplicate id
        if (seenIds.count(id) > 0) {
            SDL_Log("[TilesetLoader] Tile entry %zu: duplicate id '%s', skipping.",
                    i, id.c_str());
            continue;
        }

        // Extract source_rect
        const picojson::object* srcRectObj = nullptr;
        if (!JsonUtil::GetObject(tileObj, "source_rect", srcRectObj)) {
            SDL_Log("[TilesetLoader] Tile '%s' (entry %zu): missing 'source_rect', skipping.",
                    id.c_str(), i);
            continue;
        }

        SourceRect sr;
        if (!JsonUtil::GetInt(*srcRectObj, "x", sr.x) ||
            !JsonUtil::GetInt(*srcRectObj, "y", sr.y) ||
            !JsonUtil::GetInt(*srcRectObj, "w", sr.w) ||
            !JsonUtil::GetInt(*srcRectObj, "h", sr.h)) {
            SDL_Log("[TilesetLoader] Tile '%s' (entry %zu): source_rect missing x/y/w/h, skipping.",
                    id.c_str(), i);
            continue;
        }

        // Validate w >= 1, h >= 1
        if (sr.w < 1 || sr.h < 1) {
            SDL_Log("[TilesetLoader] Tile '%s' (entry %zu): source_rect w=%d h=%d invalid (must be >= 1), skipping.",
                    id.c_str(), i, sr.w, sr.h);
            continue;
        }

        // Validate source_rect fits within texture dimensions (only if texW/texH > 0)
        if (texW > 0 && texH > 0) {
            if (sr.x + sr.w > texW || sr.y + sr.h > texH) {
                SDL_Log("[TilesetLoader] Tile '%s' (entry %zu): source_rect (%d,%d,%d,%d) exceeds texture %dx%d, skipping.",
                        id.c_str(), i, sr.x, sr.y, sr.w, sr.h, texW, texH);
                continue;
            }
        }

        // Extract adjacency rules
        AdjacencyRules adj;
        const picojson::object* adjObj = nullptr;
        if (JsonUtil::GetObject(tileObj, "adjacency", adjObj)) {
            // Extract each direction - missing directions default to empty
            const picojson::array* dirArr = nullptr;

            if (JsonUtil::GetArray(*adjObj, "up", dirArr)) {
                ExtractStringArray(*dirArr, adj.up);
            }
            if (JsonUtil::GetArray(*adjObj, "down", dirArr)) {
                ExtractStringArray(*dirArr, adj.down);
            }
            if (JsonUtil::GetArray(*adjObj, "left", dirArr)) {
                ExtractStringArray(*dirArr, adj.left);
            }
            if (JsonUtil::GetArray(*adjObj, "right", dirArr)) {
                ExtractStringArray(*dirArr, adj.right);
            }
        }
        // If adjacency is missing, treat as empty rules (all directions empty)

        // Extract per-tile scale (optional, defaults to 1.0)
        float tileScale = 1.0f;
        {
            double scaleVal = 0.0;
            if (JsonUtil::GetDouble(tileObj, "scale", scaleVal)) {
                tileScale = static_cast<float>(scaleVal);
                if (tileScale <= 0.0f) {
                    SDL_Log("[TilesetLoader] Tile '%s' (entry %zu): invalid scale %.2f, using 1.0.",
                            id.c_str(), i, tileScale);
                    tileScale = 1.0f;
                }
            }
        }

        // Tile is valid - add it
        TileDef tileDef;
        tileDef.id = id;
        tileDef.source_rect = sr;
        tileDef.adjacency = adj;
        tileDef.scale = tileScale;

        seenIds.insert(id);
        outTiles.push_back(tileDef);
    }

    return true;
}

bool TilesetLoader::LoadTileset(SDL_Renderer* renderer, const std::string& folderPath, Tileset& out)
{
    std::string pngPath, jsonPath, name;
    FindTilesetFiles(folderPath, pngPath, jsonPath, name);

    // Load spritesheet via SDL_image (supports PNG, BMP, etc.)
    SDL_Texture* tex = IMG_LoadTexture(renderer, pngPath.c_str());
    if (!tex) {
        SDL_Log("[TilesetLoader] Failed to load texture: %s - %s",
                pngPath.c_str(), SDL_GetError());
        return false;
    }

    float fw = 0, fh = 0;
    SDL_GetTextureSize(tex, &fw, &fh);
    int texW = static_cast<int>(fw);
    int texH = static_cast<int>(fh);

    // Parse sidecar JSON
    std::vector<TileDef> tiles;
    picojson::value rawJson;
    if (!ParseSidecarJson(jsonPath, texW, texH, tiles, rawJson)) {
        SDL_DestroyTexture(tex);
        return false;
    }

    // Build output
    out.name = name;
    out.texture = tex;
    out.texture_width = texW;
    out.texture_height = texH;
    out.tiles = std::move(tiles);
    out.rawJson = rawJson;

    // Extract per-sheet scale from JSON root (optional, default 1.0)
    // Accepts both "scale" and "sheet_scale" field names for compatibility
    out.sheet_scale = 1.0f;
    if (rawJson.is<picojson::object>()) {
        const picojson::object& rootObj = rawJson.get<picojson::object>();
        double sheetScaleVal = 0.0;
        // Try "scale" first (per issue #91 spec), then "sheet_scale" for backward compat
        if (JsonUtil::GetDouble(rootObj, "scale", sheetScaleVal) ||
            JsonUtil::GetDouble(rootObj, "sheet_scale", sheetScaleVal)) {
            out.sheet_scale = static_cast<float>(sheetScaleVal);
            if (out.sheet_scale <= 0.0f) {
                SDL_Log("[TilesetLoader] Invalid sheet_scale %.2f, using 1.0.", out.sheet_scale);
                out.sheet_scale = 1.0f;
            }
        }
    }

    // Build id -> index map
    out.id_index.clear();
    for (size_t i = 0; i < out.tiles.size(); ++i) {
        out.id_index[out.tiles[i].id] = i;
    }

    return true;
}

bool TilesetLoader::LoadTilesetDef(const std::string& folderPath, TilesetDef& out)
{
    std::string pngPath, jsonPath, name;
    FindTilesetFiles(folderPath, pngPath, jsonPath, name);

    // Data-only load: skip texture loading, skip source_rect dimension validation
    // Pass texW=0, texH=0 to indicate "no dimension validation"
    std::vector<TileDef> tiles;
    picojson::value rawJson;
    if (!ParseSidecarJson(jsonPath, 0, 0, tiles, rawJson)) {
        return false;
    }

    // Build output
    out.name = name;
    out.texture_width = 0;
    out.texture_height = 0;
    out.tiles = std::move(tiles);
    out.rawJson = rawJson;

    // Extract per-sheet scale from JSON root (optional, default 1.0)
    // Accepts both "scale" and "sheet_scale" field names for compatibility
    out.sheet_scale = 1.0f;
    if (rawJson.is<picojson::object>()) {
        const picojson::object& rootObj = rawJson.get<picojson::object>();
        double sheetScaleVal = 0.0;
        // Try "scale" first (per issue #91 spec), then "sheet_scale" for backward compat
        if (JsonUtil::GetDouble(rootObj, "scale", sheetScaleVal) ||
            JsonUtil::GetDouble(rootObj, "sheet_scale", sheetScaleVal)) {
            out.sheet_scale = static_cast<float>(sheetScaleVal);
            if (out.sheet_scale <= 0.0f) {
                SDL_Log("[TilesetLoader] Invalid sheet_scale %.2f, using 1.0.", out.sheet_scale);
                out.sheet_scale = 1.0f;
            }
        }
    }

    // Build id -> index map
    out.id_index.clear();
    for (size_t i = 0; i < out.tiles.size(); ++i) {
        out.id_index[out.tiles[i].id] = i;
    }

    return true;
}
