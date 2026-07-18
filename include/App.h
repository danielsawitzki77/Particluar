#pragma once

#include <SDL3/SDL.h>
#include <string>

// Forward declarations for modules the App can host
class TileRendererDemo;

// Particluar — Top-level application class.
// Owns the SDL window and renderer. Hosts interchangeable modules
// (tile renderer demo, body renderer, future game modules).
// main.cpp creates an App instance and calls Init/Run/Shutdown.
class App {
public:
    App() = default;
    ~App() = default;

    // Initialize SDL, create window/renderer, load config, set up modules.
    bool Init();

    // Run the main loop until quit is requested.
    void Run();

    // Destroy SDL resources. Safe to call multiple times.
    void Shutdown();

    // SDL resource accessors (for modules that need them)
    SDL_Window*   GetWindow()   const { return m_window; }
    SDL_Renderer* GetRenderer() const { return m_renderer; }

private:
    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    bool m_running = false;

    // Active module — currently the tile renderer demo.
    // Future: could be switched at runtime (tile demo, body viewer, game, etc.)
    TileRendererDemo* m_tileDemo = nullptr;
};
