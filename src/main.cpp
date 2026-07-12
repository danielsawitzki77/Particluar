// Particluar — Umbrella Application
// Minimal entry point. All 2D tile/map logic lives in the TileRenderer library.

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "GlobalConfig.h"
#include "Camera.h"
#include "Viewport.h"
#include "TileWorld.h"

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    // Load config
    GlobalConfig config;
    config.Load("renderer_config.json");
    const GlobalConfigData& cfg = config.Get();

    // Create window
    SDL_Window* window = SDL_CreateWindow(
        "Particluar (WASD=scroll, Q/E=layer, +/-=zoom, ESC=quit)",
        cfg.viewport_width, cfg.viewport_height, 0);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Set up camera and viewport
    Camera camera;
    camera.SetPosition(0.0f, 0.0f);

    Viewport viewport;
    ViewportRect vpRect = { cfg.viewport_x, cfg.viewport_y, cfg.viewport_width, cfg.viewport_height };
    viewport.SetRect(vpRect);

    // Load tile world (all layers)
    TileWorld tileWorld;
    int layerCount = tileWorld.LoadTilesets(renderer, "assets/tilesets");
    SDL_Log("[Main] %d tile layers loaded.", layerCount);

    // Main loop
    float zoomLevel = 1.0f;
    const float ZOOM_MIN = 0.25f, ZOOM_MAX = 4.0f, ZOOM_STEP = 0.25f;
    const int TILES_PER_FRAME = 200;
    const float MARGIN = 128.0f;

    bool running = true;
    Uint64 lastTicks = SDL_GetTicks();

    while (running) {
        Uint64 currentTicks = SDL_GetTicks();
        float deltaTime = static_cast<float>(currentTicks - lastTicks) / 1000.0f;
        lastTicks = currentTicks;
        if (deltaTime > 0.1f) deltaTime = 0.1f;

        // Events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) running = false;
                else if (event.key.scancode == SDL_SCANCODE_Q && !event.key.repeat) tileWorld.PrevLayer();
                else if (event.key.scancode == SDL_SCANCODE_E && !event.key.repeat) tileWorld.NextLayer();
                else if ((event.key.scancode == SDL_SCANCODE_EQUALS || event.key.scancode == SDL_SCANCODE_KP_PLUS) && !event.key.repeat) {
                    zoomLevel += ZOOM_STEP; if (zoomLevel > ZOOM_MAX) zoomLevel = ZOOM_MAX;
                }
                else if ((event.key.scancode == SDL_SCANCODE_MINUS || event.key.scancode == SDL_SCANCODE_KP_MINUS) && !event.key.repeat) {
                    zoomLevel -= ZOOM_STEP; if (zoomLevel < ZOOM_MIN) zoomLevel = ZOOM_MIN;
                }
            }
            else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                if (event.wheel.y > 0) { zoomLevel += ZOOM_STEP; if (zoomLevel > ZOOM_MAX) zoomLevel = ZOOM_MAX; }
                else if (event.wheel.y < 0) { zoomLevel -= ZOOM_STEP; if (zoomLevel < ZOOM_MIN) zoomLevel = ZOOM_MIN; }
            }
        }

        // Update
        const bool* keyState = SDL_GetKeyboardState(NULL);
        camera.Update(deltaTime, cfg.scroll_speed, keyState);
        tileWorld.Update(camera, viewport, zoomLevel, TILES_PER_FRAME, MARGIN);

        // Render
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);
        tileWorld.Render(renderer, camera, viewport, zoomLevel, static_cast<Uint32>(currentTicks));
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
