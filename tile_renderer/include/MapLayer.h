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
    int zDepth;                 // draw order (lower = drawn first / behind)
    Uint8 alpha;                // 0-255
    float pivotX, pivotY;       // normalized 0-1, default (0.5, 0.5)
    float offsetX, offsetY;     // pixel offset
    float scale;                // runtime layer scale, default 1.0
    SamplingMode sampling;      // default Nearest

    MapLayerConfig()
        : zDepth(0)
        , alpha(255)
        , pivotX(0.5f), pivotY(0.5f)
        , offsetX(0.0f), offsetY(0.0f)
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
