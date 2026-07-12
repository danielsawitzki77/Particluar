#pragma once

#include "StreamingMapGenerator.h"
#include "TilesetLoader.h"
#include "TileRenderer.h"
#include "Camera.h"
#include "Viewport.h"
#include "MapLayer.h"

#include <SDL3/SDL.h>
#include <vector>
#include <string>

// High-level manager for the 2D tile world.
// Owns multiple tileset layers, each with its own StreamingMapGenerator.
// Provides a simple Update/Render interface for the application.
class TileWorld {
public:
    // Load all tilesets from a directory. Returns number of layers created.
    int LoadTilesets(SDL_Renderer* renderer, const std::string& rootDir);

    // Update streaming generation for all layers based on current camera/viewport.
    // budget = max cells to generate across all layers this frame.
    // margin = extra pixels beyond viewport to pre-generate.
    void Update(const Camera& camera, const Viewport& viewport,
                float zoomLevel, int budget, float margin);

    // Render the active layer.
    void Render(SDL_Renderer* renderer, const Camera& camera, const Viewport& viewport,
                float zoomLevel, Uint32 elapsedMs);

    // Layer switching
    int GetLayerCount() const { return static_cast<int>(m_layers.size()); }
    int GetActiveLayer() const { return m_activeLayer; }
    void SetActiveLayer(int idx);
    void NextLayer();
    void PrevLayer();
    const std::string& GetActiveLayerName() const;

private:
    struct Layer {
        Tileset tileset;
        TilesetDef def;
        StreamingMapGenerator generator;
    };

    std::vector<Layer> m_layers;
    int m_activeLayer = 0;
    TileRenderer m_tileRenderer;
};
