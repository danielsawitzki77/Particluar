// Particluar — 2D Map System PoC
// Tasks 9 & 11: WASD Camera Scrolling + Real-Time WFC Generation (G key)

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "GlobalConfig.h"
#include "Camera.h"
#include "Viewport.h"
#include "TileRenderer.h"
#include "TilesetLoader.h"
#include "MapLoader.h"
#include "WFCGenerator.h"

// ---------------------------------------------------------------------------
// Procedural test tileset creation (in-memory, no file required)
// Creates a 64x64 surface with 4 colored 32x32 tiles: red, green, blue, yellow
// ---------------------------------------------------------------------------
static bool CreateTestTileset(SDL_Renderer* renderer, Tileset& out)
{
    const int TILE_SIZE = 32;
    const int TEX_W = 64;
    const int TEX_H = 64;

    SDL_Surface* surface = SDL_CreateSurface(TEX_W, TEX_H, SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        SDL_Log("[PoC] Failed to create test surface: %s", SDL_GetError());
        return false;
    }

    // Fill 4 quadrants with distinct colors
    SDL_Rect rects[4] = {
        { 0,  0,  TILE_SIZE, TILE_SIZE },  // top-left: red
        { TILE_SIZE, 0,  TILE_SIZE, TILE_SIZE },  // top-right: green
        { 0,  TILE_SIZE, TILE_SIZE, TILE_SIZE },  // bottom-left: blue
        { TILE_SIZE, TILE_SIZE, TILE_SIZE, TILE_SIZE }   // bottom-right: yellow
    };

    Uint32 colors[4];
    colors[0] = SDL_MapSurfaceRGBA(surface, 200, 50, 50, 255);    // red
    colors[1] = SDL_MapSurfaceRGBA(surface, 50, 200, 50, 255);    // green
    colors[2] = SDL_MapSurfaceRGBA(surface, 50, 50, 200, 255);    // blue
    colors[3] = SDL_MapSurfaceRGBA(surface, 200, 200, 50, 255);   // yellow

    for (int i = 0; i < 4; ++i) {
        SDL_FillSurfaceRect(surface, &rects[i], colors[i]);
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);

    if (!tex) {
        SDL_Log("[PoC] Failed to create test texture: %s", SDL_GetError());
        return false;
    }

    // Build tileset struct
    out.name = "test";
    out.texture = tex;
    out.texture_width = TEX_W;
    out.texture_height = TEX_H;

    // Define 4 tiles with adjacency rules allowing all neighbors
    std::vector<std::string> allIds = { "red", "green", "blue", "yellow" };

    TileDef redTile;
    redTile.id = "red";
    redTile.source_rect = { 0, 0, TILE_SIZE, TILE_SIZE };
    redTile.adjacency = { allIds, allIds, allIds, allIds };

    TileDef greenTile;
    greenTile.id = "green";
    greenTile.source_rect = { TILE_SIZE, 0, TILE_SIZE, TILE_SIZE };
    greenTile.adjacency = { allIds, allIds, allIds, allIds };

    TileDef blueTile;
    blueTile.id = "blue";
    blueTile.source_rect = { 0, TILE_SIZE, TILE_SIZE, TILE_SIZE };
    blueTile.adjacency = { allIds, allIds, allIds, allIds };

    TileDef yellowTile;
    yellowTile.id = "yellow";
    yellowTile.source_rect = { TILE_SIZE, TILE_SIZE, TILE_SIZE, TILE_SIZE };
    yellowTile.adjacency = { allIds, allIds, allIds, allIds };

    out.tiles.push_back(redTile);
    out.tiles.push_back(greenTile);
    out.tiles.push_back(blueTile);
    out.tiles.push_back(yellowTile);

    // Build id -> index map
    out.id_index.clear();
    for (size_t i = 0; i < out.tiles.size(); ++i) {
        out.id_index[out.tiles[i].id] = i;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Build a TilesetDef from the Tileset (for WFC which uses TilesetDef)
// ---------------------------------------------------------------------------
static TilesetDef BuildTilesetDef(const Tileset& ts)
{
    TilesetDef def;
    def.name = ts.name;
    def.texture_width = ts.texture_width;
    def.texture_height = ts.texture_height;
    def.tiles = ts.tiles;
    def.id_index = ts.id_index;
    return def;
}

// ---------------------------------------------------------------------------
// Generate a simple random map as the initial map (before WFC is triggered)
// ---------------------------------------------------------------------------
static MapData CreateInitialMap(int width, int height, const Tileset& tileset)
{
    MapData map;
    map.width = width;
    map.height = height;
    map.tileset_id = tileset.name;
    map.grid.resize(height);

    int numTiles = static_cast<int>(tileset.tiles.size());
    unsigned int seed = 12345;

    for (int r = 0; r < height; ++r) {
        map.grid[r].resize(width);
        for (int c = 0; c < width; ++c) {
            // Simple deterministic pseudo-random tile selection
            seed = seed * 1103515245 + 12345;
            int idx = static_cast<int>((seed >> 16) % numTiles);
            map.grid[r][c] = tileset.tiles[idx].id;
        }
    }

    return map;
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // --- SDL Init ---
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    // --- Load Global Config ---
    GlobalConfig config;
    if (!config.Load("renderer_config.json")) {
        SDL_Log("[PoC] Config load failed or not found; using defaults.");
    }
    const GlobalConfigData& cfg = config.Get();

    // --- Create Window and Renderer ---
    SDL_Window* window = SDL_CreateWindow(
        "Particluar - 2D Map PoC (WASD=scroll, G=generate)",
        cfg.viewport_width, cfg.viewport_height,
        0);

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

    // --- Create Test Tileset ---
    Tileset tileset;
    if (!CreateTestTileset(renderer, tileset)) {
        SDL_Log("[PoC] Failed to create test tileset.");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    TilesetDef tilesetDef = BuildTilesetDef(tileset);

    // --- Create Initial Map ---
    const int MAP_WIDTH = 64;
    const int MAP_HEIGHT = 64;
    MapData activeMap = CreateInitialMap(MAP_WIDTH, MAP_HEIGHT, tileset);

    // --- Set Up Renderer Components ---
    Camera camera;
    camera.SetPosition(0.0f, 0.0f);

    Viewport viewport;
    ViewportRect vpRect;
    vpRect.x = cfg.viewport_x;
    vpRect.y = cfg.viewport_y;
    vpRect.width = cfg.viewport_width;
    vpRect.height = cfg.viewport_height;
    viewport.SetRect(vpRect);

    TileRenderer tileRenderer;

    // --- WFC Generator ---
    WFCGenerator wfcGenerator;

    // --- Main Loop ---
    bool running = true;
    Uint64 lastTicks = SDL_GetTicks();

    SDL_Log("[PoC] Running. WASD=scroll, G=regenerate map via WFC, ESC/close=quit");

    while (running) {
        // --- Delta time ---
        Uint64 currentTicks = SDL_GetTicks();
        float deltaTime = static_cast<float>(currentTicks - lastTicks) / 1000.0f;
        lastTicks = currentTicks;

        // Clamp delta to avoid huge jumps (e.g., after breakpoint)
        if (deltaTime > 0.1f) {
            deltaTime = 0.1f;
        }

        // --- Event Polling ---
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                    running = false;
                }
                // Task 11.1: G-key triggers WFC generation
                else if (event.key.scancode == SDL_SCANCODE_G && !event.key.repeat) {
                    SDL_Log("[PoC] Generating new map via WFC (64x64)...");

                    Uint64 genStart = SDL_GetTicks();

                    WFCParams params;
                    params.width = MAP_WIDTH;
                    params.height = MAP_HEIGHT;
                    params.seed = 0; // non-deterministic (random)
                    params.tileset = &tilesetDef;

                    WFCResult result = wfcGenerator.Generate(params);

                    Uint64 genEnd = SDL_GetTicks();
                    float genTime = static_cast<float>(genEnd - genStart) / 1000.0f;

                    if (result.status == WFCStatus::Success) {
                        // Task 11.2: On success, replace active map
                        activeMap = result.map;
                        SDL_Log("[PoC] WFC generation succeeded in %.3f seconds.", genTime);
                    }
                    else if (result.status == WFCStatus::Contradiction) {
                        // Task 11.2: On contradiction, keep previous map
                        SDL_Log("[PoC] WFC contradiction after %.3f seconds. Keeping previous map.", genTime);
                    }
                    else {
                        SDL_Log("[PoC] WFC invalid input after %.3f seconds.", genTime);
                    }
                }
            }
        }

        // --- Camera Update (Task 9.3) ---
        const bool* keyState = SDL_GetKeyboardState(NULL);
        camera.Update(deltaTime, cfg.scroll_speed, keyState);

        // --- Render ---
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);

        // Task 9.4: Compute visible tiles and render
        VisibleTileRange visibleRange = viewport.ComputeVisibleTiles(
            camera.GetX(), camera.GetY(),
            camera.GetPivotX(), camera.GetPivotY(),
            cfg.tile_width, cfg.tile_height);

        tileRenderer.RenderLayer(
            renderer,
            tileset,
            activeMap,
            visibleRange,
            viewport,
            camera,
            cfg.tile_width, cfg.tile_height,
            0,    // layer 0
            255   // full opacity
        );

        SDL_RenderPresent(renderer);
    }

    // --- Cleanup ---
    if (tileset.texture) {
        SDL_DestroyTexture(tileset.texture);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    SDL_Log("[PoC] Shutdown complete.");
    return 0;
}
