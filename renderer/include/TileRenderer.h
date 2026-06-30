#pragma once

#include "Viewport.h"
#include "Camera.h"
#include "TilesetLoader.h"
#include "MapLoader.h"
#include "MapLayer.h"
#include <SDL3/SDL.h>
#include <vector>

class TileRenderer {
public:
    // Render all visible tiles for a single layer. (backward-compatible)
    void RenderLayer(
        SDL_Renderer* renderer,
        const Tileset& tileset,
        const MapData& mapData,
        const VisibleTileRange& range,
        const Viewport& viewport,
        const Camera& camera,
        int tile_width, int tile_height,  // grid cell size in pixels
        int layer,                        // Z-depth layer index
        Uint8 alpha                       // 0-255 transparency
    );

    // Render multiple independent layers sorted by z_depth.
    // Each layer has its own tileset, map data, scale, alpha, pivot, offset, and sampling.
    // base_tile_width/height is the logical grid cell size before any scaling.
    void RenderLayers(
        SDL_Renderer* renderer,
        const std::vector<MapLayer>& layers,
        const Viewport& viewport,
        const Camera& camera,
        int base_tile_width, int base_tile_height
    );

    // Set the fallback tile color for unresolved tile IDs.
    void SetFallbackColor(Uint8 r, Uint8 g, Uint8 b);

private:
    Uint8 m_fallback_r = 255, m_fallback_g = 0, m_fallback_b = 255; // magenta
};
