#pragma once

#include "MapLoader.h"
#include "TilesetLoader.h"
#include "Viewport.h"
#include "Camera.h"
#include <SDL3/SDL.h>
#include <vector>
#include <algorithm>

enum class SamplingMode {
    Nearest,  // SDL_SCALEMODE_NEAREST — pixelated
    Linear    // SDL_SCALEMODE_LINEAR — smooth/anti-aliased
};

struct MapLayerConfig {
    int z_depth;                // draw order (lower = drawn first / behind)
    Uint8 alpha;                // 0-255
    float pivot_x, pivot_y;     // normalized 0-1, default (0.5, 0.5)
    float offset_x, offset_y;   // pixel offset
    float scale;                // runtime layer scale, default 1.0
    SamplingMode sampling;      // default Nearest

    MapLayerConfig()
        : z_depth(0)
        , alpha(255)
        , pivot_x(0.5f), pivot_y(0.5f)
        , offset_x(0.0f), offset_y(0.0f)
        , scale(1.0f)
        , sampling(SamplingMode::Nearest)
    {}
};

class MapLayer {
public:
    MapLayer();

    void SetConfig(const MapLayerConfig& config);
    const MapLayerConfig& GetConfig() const;

    void SetMapData(const MapData& mapData);
    const MapData& GetMapData() const;

    void SetTileset(const Tileset* tileset);
    const Tileset* GetTileset() const;

private:
    MapLayerConfig m_config;
    MapData m_mapData;
    const Tileset* m_tileset;  // non-owning pointer
};
