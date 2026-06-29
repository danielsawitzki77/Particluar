#pragma once

#include "Viewport.h"
#include "Camera.h"
#include "TilesetLoader.h"
#include "MapLoader.h"
#include <SDL3/SDL.h>

class TileRenderer {
public:
    // Render all visible tiles for a single layer.
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

    // Set the fallback tile color for unresolved tile IDs.
    void SetFallbackColor(Uint8 r, Uint8 g, Uint8 b);

private:
    Uint8 m_fallback_r = 255, m_fallback_g = 0, m_fallback_b = 255; // magenta
};
