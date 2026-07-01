// Particluar — 2D Map System PoC
// Tasks 9 & 11: WASD Camera Scrolling + Real-Time WFC Generation (G key)
// Issue #91: Multi-Layer Rendering with Three-Level Scaling
// Task 10: Jigsaw PoC integration (J key)
// Issue #94: Animated tile support (water tiles animate at runtime)

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "GlobalConfig.h"
#include "Camera.h"
#include "Viewport.h"
#include "TileRenderer.h"
#include "TilesetLoader.h"
#include "MapLoader.h"
#include "MapLayer.h"
#include "WFCGenerator.h"
#include "ChunkManager.h"

#include <windows.h>
#include <vector>
#include <string>

// ---------------------------------------------------------------------------
// Discover all JSON sidecar files recursively under assets/tilesets/
// Returns paths like "assets/tilesets/grassland/ground_grasss.json"
// ---------------------------------------------------------------------------
static std::vector<std::string> FindAllTilesetJsons(const std::string& rootDir)
{
    std::vector<std::string> results;
    std::vector<std::string> dirs;
    dirs.push_back(rootDir);

    while (!dirs.empty()) {
        std::string dir = dirs.back();
        dirs.pop_back();

        WIN32_FIND_DATAA fd;
        std::string pattern = dir + "\\*";
        HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE) continue;

        do {
            std::string name = fd.cFileName;
            if (name == "." || name == "..") continue;
            std::string fullPath = dir + "\\" + name;

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                dirs.push_back(fullPath);
            } else {
                // Check if it's a .json file with a matching .png sibling
                if (name.size() > 5 && name.substr(name.size() - 5) == ".json") {
                    std::string baseName = name.substr(0, name.size() - 5);
                    std::string pngPath = dir + "\\" + baseName + ".png";
                    // Check PNG exists
                    DWORD attr = GetFileAttributesA(pngPath.c_str());
                    if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                        // Convert backslashes to forward slashes for SDL
                        std::string result = fullPath;
                        for (char& c : result) { if (c == '\\') c = '/'; }
                        results.push_back(result);
                    }
                }
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }

    return results;
}

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
        "Particluar - Multi-Layer PoC (WASD=scroll, G=grid WFC, J=jigsaw WFC)",
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

    // --- Auto-discover and load all tilesets with JSON sidecars ---
    std::vector<Tileset> allTilesets;
    TilesetLoader tilesetLoader;
    {
        std::vector<std::string> jsonPaths = FindAllTilesetJsons("assets/tilesets");
        SDL_Log("[PoC] Found %d tileset JSON(s) in assets/tilesets/", (int)jsonPaths.size());
        for (const std::string& jsonPath : jsonPaths) {
            Tileset ts;
            if (tilesetLoader.LoadTilesetFromJson(renderer, jsonPath, ts)) {
                SDL_Log("[PoC]   Loaded: %s (%d tiles)", jsonPath.c_str(), (int)ts.tiles.size());
                allTilesets.push_back(ts);
            } else {
                SDL_Log("[PoC]   Failed: %s", jsonPath.c_str());
            }
        }
    }

    // Fallback: procedural test tileset if nothing was found
    if (allTilesets.empty()) {
        SDL_Log("[PoC] No disk tilesets found, using procedural fallback.");
        Tileset procTs;
        if (CreateTestTileset(renderer, procTs)) {
            allTilesets.push_back(procTs);
        } else {
            SDL_Log("[PoC] Failed to create test tileset.");
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
    }

    // Use the first loaded tileset for the single-layer demo and WFC
    Tileset& tileset = allTilesets[0];
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

    // --- Jigsaw Map (Task 10: PoC integration) ---
    JigsawMap jigsawMap;
    bool useJigsawRendering = false;

    // --- Main Loop ---
    bool running = true;
    Uint64 lastTicks = SDL_GetTicks();

    SDL_Log("[PoC] Running. WASD=scroll, G=grid WFC, J=jigsaw WFC, ESC/close=quit (2 layers active)");

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
                    useJigsawRendering = false;

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
                // Task 10: J-key triggers jigsaw WFC generation on a 512x512 area
                else if (event.key.scancode == SDL_SCANCODE_J && !event.key.repeat) {
                    SDL_Log("[PoC] Generating jigsaw map (512x512)...");

                    Uint64 genStart = SDL_GetTicks();

                    JigsawWFCParams jParams;
                    jParams.target_width = 512.0f;
                    jParams.target_height = 512.0f;
                    jParams.origin_x = 0.0f;
                    jParams.origin_y = 0.0f;
                    jParams.seed = 0; // non-deterministic
                    jParams.tileset = &tilesetDef;
                    jParams.layer_scale = 1.0f;

                    JigsawWFCResult jResult = wfcGenerator.GenerateJigsaw(jParams);

                    Uint64 genEnd = SDL_GetTicks();
                    float genTime = static_cast<float>(genEnd - genStart) / 1000.0f;

                    if (jResult.status == WFCStatus::Success) {
                        jigsawMap = jResult.map;
                        useJigsawRendering = true;
                        SDL_Log("[PoC] Jigsaw generation succeeded in %.3f seconds (%d tiles).",
                                genTime, (int)jigsawMap.GetTileCount());
                    }
                    else if (jResult.status == WFCStatus::Contradiction) {
                        SDL_Log("[PoC] Jigsaw WFC contradiction after %.3f seconds.", genTime);
                    }
                    else {
                        SDL_Log("[PoC] Jigsaw WFC invalid input after %.3f seconds.", genTime);
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

        // Elapsed time for tile animation (absolute ms since SDL init)
        Uint32 elapsed_ms = static_cast<Uint32>(currentTicks);

        if (useJigsawRendering) {
            // Task 10: Render jigsaw map using RenderJigsawLayer
            MapLayerConfig jigsawCfg;
            jigsawCfg.z_depth = 0;
            jigsawCfg.alpha = 255;
            jigsawCfg.pivot_x = camera.GetPivotX();
            jigsawCfg.pivot_y = camera.GetPivotY();
            jigsawCfg.offset_x = 0.0f;
            jigsawCfg.offset_y = 0.0f;
            jigsawCfg.scale = 1.0f;
            jigsawCfg.sampling = SamplingMode::Nearest;

            tileRenderer.RenderJigsawLayer(
                renderer, tileset, jigsawMap, viewport, camera, jigsawCfg, elapsed_ms);
        } else {
            // Legacy grid-based rendering (G-key / initial map)
            // Issue #91: Multi-layer rendering demonstration
            // Layer 0: Base ground layer (full opacity, no offset, nearest sampling)
            MapLayer baseLayer;
            {
                MapLayerConfig layerCfg;
                layerCfg.z_depth = 0;
                layerCfg.alpha = 255;
                layerCfg.pivot_x = camera.GetPivotX();
                layerCfg.pivot_y = camera.GetPivotY();
                layerCfg.offset_x = 0.0f;
                layerCfg.offset_y = 0.0f;
                layerCfg.scale = 1.0f;
                layerCfg.sampling = SamplingMode::Nearest;
                baseLayer.SetConfig(layerCfg);
                baseLayer.SetMapData(activeMap);
                baseLayer.SetTileset(&tileset);
            }

            // Layer 1: Overlay/detail layer — same map, offset, semi-transparent, linear sampling
            MapLayer overlayLayer;
            {
                MapLayerConfig layerCfg;
                layerCfg.z_depth = 1;
                layerCfg.alpha = 100;
                layerCfg.pivot_x = camera.GetPivotX();
                layerCfg.pivot_y = camera.GetPivotY();
                layerCfg.offset_x = 16.0f;
                layerCfg.offset_y = 16.0f;
                layerCfg.scale = 1.0f;
                layerCfg.sampling = SamplingMode::Linear;
                overlayLayer.SetConfig(layerCfg);
                overlayLayer.SetMapData(activeMap);
                overlayLayer.SetTileset(&tileset);
            }

            std::vector<MapLayer> layers;
            layers.push_back(baseLayer);
            layers.push_back(overlayLayer);

            tileRenderer.RenderLayers(
                renderer,
                layers,
                viewport,
                camera,
                cfg.tile_width, cfg.tile_height,
                elapsed_ms
            );
        }

        SDL_RenderPresent(renderer);
    }

    // --- Cleanup ---
    for (Tileset& ts : allTilesets) {
        if (ts.texture) SDL_DestroyTexture(ts.texture);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    SDL_Log("[PoC] Shutdown complete.");
    return 0;
}
