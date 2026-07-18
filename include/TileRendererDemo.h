#pragma once

#include "GlobalConfig.h"
#include "Camera.h"
#include "Viewport.h"
#include "TileWorld.h"

#include <SDL3/SDL.h>

// Encapsulates the tile renderer test/demo functionality.
// Loads tilesets, handles input for scrolling/zoom/layer switching,
// and renders the streaming tile world.
// Instantiated by App as one of its hosted modules.
class TileRendererDemo {
public:
    // Load config and tilesets. Returns false on failure.
    bool Init(SDL_Renderer* renderer);

    // Process a single SDL event. Returns true if the event signals quit.
    bool HandleEvent(const SDL_Event& event);

    // Per-frame update: camera movement and streaming tile generation.
    void Update(float deltaTime);

    // Render the tile world.
    void Render(SDL_Renderer* renderer, Uint32 elapsedMs);

private:
    GlobalConfig m_config;
    Camera m_camera;
    Viewport m_viewport;
    TileWorld m_tileWorld;

    float m_zoomLevel = 1.0f;

    static constexpr float ZOOM_MIN = 0.25f;
    static constexpr float ZOOM_MAX = 4.0f;
    static constexpr float ZOOM_STEP = 0.25f;
    static constexpr int   TILES_PER_FRAME = 200;
    static constexpr float MARGIN = 128.0f;
};
