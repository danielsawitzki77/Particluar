#include "TileRendererDemo.h"

bool TileRendererDemo::Init(SDL_Renderer* renderer)
{
    m_config.Load("tile_renderer/TileRendererConfig.json");
    const GlobalConfigData& cfg = m_config.Get();

    m_camera.SetPosition(0.0f, 0.0f);

    ViewportRect vpRect = { cfg.viewportX, cfg.viewportY, cfg.viewportWidth, cfg.viewportHeight };
    m_viewport.SetRect(vpRect);

    int layerCount = m_tileWorld.LoadTilesets(renderer, "assets/tilesets");
    SDL_Log("[TileRendererDemo] %d tile layers loaded.", layerCount);

    return layerCount > 0;
}

bool TileRendererDemo::HandleEvent(const SDL_Event& event)
{
    if (event.type == SDL_EVENT_QUIT)
        return true;

    if (event.type == SDL_EVENT_KEY_DOWN) {
        if (event.key.scancode == SDL_SCANCODE_ESCAPE)
            return true;
        if (event.key.scancode == SDL_SCANCODE_Q && !event.key.repeat)
            m_tileWorld.PrevLayer();
        if (event.key.scancode == SDL_SCANCODE_E && !event.key.repeat)
            m_tileWorld.NextLayer();
        if ((event.key.scancode == SDL_SCANCODE_EQUALS || event.key.scancode == SDL_SCANCODE_KP_PLUS) && !event.key.repeat) {
            m_zoomLevel += ZOOM_STEP;
            if (m_zoomLevel > ZOOM_MAX) m_zoomLevel = ZOOM_MAX;
        }
        if ((event.key.scancode == SDL_SCANCODE_MINUS || event.key.scancode == SDL_SCANCODE_KP_MINUS) && !event.key.repeat) {
            m_zoomLevel -= ZOOM_STEP;
            if (m_zoomLevel < ZOOM_MIN) m_zoomLevel = ZOOM_MIN;
        }
    }

    if (event.type == SDL_EVENT_MOUSE_WHEEL) {
        if (event.wheel.y > 0) {
            m_zoomLevel += ZOOM_STEP;
            if (m_zoomLevel > ZOOM_MAX) m_zoomLevel = ZOOM_MAX;
        } else if (event.wheel.y < 0) {
            m_zoomLevel -= ZOOM_STEP;
            if (m_zoomLevel < ZOOM_MIN) m_zoomLevel = ZOOM_MIN;
        }
    }

    return false;
}

void TileRendererDemo::Update(float deltaTime)
{
    const GlobalConfigData& cfg = m_config.Get();
    const bool* keyState = SDL_GetKeyboardState(NULL);
    m_camera.Update(deltaTime, cfg.scrollSpeed, keyState);
    m_tileWorld.Update(m_camera, m_viewport, m_zoomLevel, TILES_PER_FRAME, MARGIN);
}

void TileRendererDemo::Render(SDL_Renderer* renderer, Uint32 elapsedMs)
{
    const GlobalConfigData& cfg = m_config.Get();
    m_tileWorld.Render(renderer, m_camera, m_viewport, m_zoomLevel, elapsedMs, cfg.debugShowBlocking);
}
