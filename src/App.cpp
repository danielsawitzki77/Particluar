#include "App.h"
#include "TileRendererDemo.h"
#include "GlobalConfig.h"

bool App::Init()
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    // Load config for window dimensions
    GlobalConfig config;
    config.Load("tile_renderer/TileRendererConfig.json");
    const GlobalConfigData& cfg = config.Get();

    m_window = SDL_CreateWindow(
        "Particluar (WASD=scroll, Q/E=layer, +/-=zoom, ESC=quit)",
        cfg.viewportWidth, cfg.viewportHeight, 0);
    if (!m_window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    m_renderer = SDL_CreateRenderer(m_window, NULL);
    if (!m_renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
        SDL_Quit();
        return false;
    }

    // Initialize the tile renderer demo module
    m_tileDemo = new TileRendererDemo();
    if (!m_tileDemo->Init(m_renderer)) {
        SDL_Log("[App] TileRendererDemo init failed.");
        delete m_tileDemo;
        m_tileDemo = nullptr;
        // Non-fatal: app can still run, just nothing to render
    }

    return true;
}

void App::Run()
{
    m_running = true;
    Uint64 lastTicks = SDL_GetTicks();

    while (m_running) {
        Uint64 currentTicks = SDL_GetTicks();
        float deltaTime = static_cast<float>(currentTicks - lastTicks) / 1000.0f;
        lastTicks = currentTicks;
        if (deltaTime > 0.1f) deltaTime = 0.1f;

        // Events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (m_tileDemo && m_tileDemo->HandleEvent(event)) {
                m_running = false;
            }
        }

        // Update
        if (m_tileDemo) {
            m_tileDemo->Update(deltaTime);
        }

        // Render
        SDL_SetRenderDrawColor(m_renderer, 30, 30, 30, 255);
        SDL_RenderClear(m_renderer);

        if (m_tileDemo) {
            m_tileDemo->Render(m_renderer, static_cast<Uint32>(currentTicks));
        }

        SDL_RenderPresent(m_renderer);
    }
}

void App::Shutdown()
{
    delete m_tileDemo;
    m_tileDemo = nullptr;

    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    SDL_Quit();
}
