#pragma once

#include "JsonUtil.h"
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include <map>

// Forward-declare SDL_Texture for the Tileset struct
struct SDL_Texture;
struct SDL_Renderer;

struct SourceRect {
    int x, y, w, h;  // w >= 1, h >= 1
};

struct AdjacencyRules {
    std::vector<std::string> up;
    std::vector<std::string> down;
    std::vector<std::string> left;
    std::vector<std::string> right;
};

struct AnimationFrame {
    int tileid;        // tile ID within the tileset (used to compute source_rect)
    int duration_ms;   // frame display duration in milliseconds
    SourceRect rect;   // resolved source rect for this frame
};

struct TileDef {
    std::string id;            // unique within tileset, 1-64 chars
    SourceRect source_rect;
    AdjacencyRules adjacency;
    float scale;               // per-tile scale from JSON, default 1.0
    std::vector<AnimationFrame> animation; // empty = static tile

    TileDef() : scale(1.0f) {}

    // Returns the source rect for the current frame given elapsed time (ms).
    // If no animation, returns the static source_rect.
    SourceRect GetCurrentRect(Uint32 elapsed_ms) const {
        if (animation.empty()) return source_rect;
        // Compute total animation cycle duration
        int total = 0;
        for (const auto& f : animation) total += f.duration_ms;
        if (total <= 0) return source_rect;
        int t = static_cast<int>(elapsed_ms % static_cast<Uint32>(total));
        int accum = 0;
        for (const auto& f : animation) {
            accum += f.duration_ms;
            if (t < accum) return f.rect;
        }
        return animation.back().rect;
    }
};

// Pure data version (no SDL_Texture) for CLI tools and WFC generator
struct TilesetDef {
    std::string name;
    int texture_width;     // 0 = not validated (LoadTilesetDef doesn't load texture)
    int texture_height;    // 0 = not validated
    float sheet_scale;     // per-sheet scale from JSON root, default 1.0
    std::vector<TileDef> tiles;
    std::map<std::string, size_t> id_index; // id -> index in tiles vector
    picojson::value rawJson; // preserved for round-trip fidelity

    TilesetDef() : texture_width(0), texture_height(0), sheet_scale(1.0f) {}
};

// Full version with texture for runtime rendering
struct Tileset {
    std::string name;
    SDL_Texture* texture;
    int texture_width, texture_height;
    float sheet_scale;     // per-sheet scale from JSON root, default 1.0
    std::vector<TileDef> tiles;
    std::map<std::string, size_t> id_index; // id -> index in tiles vector
    picojson::value rawJson; // preserved for round-trip fidelity

    Tileset() : texture(nullptr), texture_width(0), texture_height(0), sheet_scale(1.0f) {}
};

class TilesetLoader {
public:
    // Full load with texture (requires SDL_Renderer). For runtime rendering.
    bool LoadTileset(SDL_Renderer* renderer, const std::string& folderPath, Tileset& out);

    // Load tileset from a specific JSON sidecar path.
    // Derives the PNG path from the JSON's base name in the same directory.
    bool LoadTilesetFromJson(SDL_Renderer* renderer, const std::string& jsonPath, Tileset& out);

    // Data-only load (no texture). For CLI tools and WFC.
    bool LoadTilesetDef(const std::string& folderPath, TilesetDef& out);

private:
    bool ParseSidecarJson(const std::string& jsonPath, int texW, int texH,
                          std::vector<TileDef>& outTiles, picojson::value& rawJson);

    // Parse TSX (Tiled tileset XML) files in a folder to extract animation data.
    // Call after tiles are loaded to augment TileDefs with animation frames.
    void ParseTsxAnimations(const std::string& folderPath, std::vector<TileDef>& tiles,
                            int texW, int columns);
};
