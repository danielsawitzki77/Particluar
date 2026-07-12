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
#include <set>
#include <random>
#include <cmath>

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
    out.textureWidth = TEX_W;
    out.textureHeight = TEX_H;

    // Define 4 tiles with adjacency rules allowing all neighbors
    std::vector<std::string> allIds = { "red", "green", "blue", "yellow" };

    TileDef redTile;
    redTile.id = "red";
    redTile.sourceRect = { 0, 0, TILE_SIZE, TILE_SIZE };
    redTile.adjacency = { allIds, allIds, allIds, allIds };

    TileDef greenTile;
    greenTile.id = "green";
    greenTile.sourceRect = { TILE_SIZE, 0, TILE_SIZE, TILE_SIZE };
    greenTile.adjacency = { allIds, allIds, allIds, allIds };

    TileDef blueTile;
    blueTile.id = "blue";
    blueTile.sourceRect = { 0, TILE_SIZE, TILE_SIZE, TILE_SIZE };
    blueTile.adjacency = { allIds, allIds, allIds, allIds };

    TileDef yellowTile;
    yellowTile.id = "yellow";
    yellowTile.sourceRect = { TILE_SIZE, TILE_SIZE, TILE_SIZE, TILE_SIZE };
    yellowTile.adjacency = { allIds, allIds, allIds, allIds };

    out.tiles.push_back(redTile);
    out.tiles.push_back(greenTile);
    out.tiles.push_back(blueTile);
    out.tiles.push_back(yellowTile);

    // Build id -> index map
    out.idIndex.clear();
    for (size_t i = 0; i < out.tiles.size(); ++i) {
        out.idIndex[out.tiles[i].id] = i;
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
    def.textureWidth = ts.textureWidth;
    def.textureHeight = ts.textureHeight;
    def.tiles = ts.tiles;
    def.idIndex = ts.idIndex;
    return def;
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
        "Particluar - PoC (WASD=scroll, Q/E=tileset, +/-=zoom, G=WFC, J=jigsaw)",
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

    // Use the first loaded tileset, switchable with Q/E
    int activeTilesetIdx = 0;
    Tileset* tileset = &allTilesets[activeTilesetIdx];
    TilesetDef tilesetDef = BuildTilesetDef(*tileset);

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

    // --- Zoom level (+ and - keys, or mouse wheel) ---
    float zoomLevel = 1.0f;
    const float ZOOM_MIN = 0.25f;
    const float ZOOM_MAX = 4.0f;
    const float ZOOM_STEP = 0.25f;

    // --- Streaming tile generation (all layers) ---
    // Each tileset becomes a layer with its own JigsawMap and generation state.
    struct LayerState {
        TilesetDef def;
        Tileset* tileset;
        JigsawMap map;
        std::set<std::pair<int, int>> generatedCells; // (col, row) of generated cells
        float cellW, cellH; // tile cell size for this layer
        std::mt19937 rng;
    };
    std::vector<LayerState> layers;
    for (size_t i = 0; i < allTilesets.size(); ++i) {
        LayerState layer;
        layer.tileset = &allTilesets[i];
        layer.def = BuildTilesetDef(*layer.tileset);
        layer.map.SetTilesetId(layer.def.name);

        // Determine cell size from first tile
        float sheetScale = layer.def.sheetScale;
        if (!layer.def.tiles.empty()) {
            const TileDef& ft = layer.def.tiles[0];
            layer.cellW = static_cast<float>(ft.sourceRect.w) * sheetScale * ft.scale;
            layer.cellH = static_cast<float>(ft.sourceRect.h) * sheetScale * ft.scale;
        } else {
            layer.cellW = 16.0f;
            layer.cellH = 16.0f;
        }
        if (layer.cellW < 1.0f) layer.cellW = 1.0f;
        if (layer.cellH < 1.0f) layer.cellH = 1.0f;

        std::random_device rd;
        layer.rng.seed(rd());

        layers.push_back(std::move(layer));
    }

    // Per-frame tile generation budget
    const int TILES_PER_FRAME = 80;
    // Margin around viewport (in pixels) to pre-generate
    const float MARGIN = 64.0f;

    // Which layer is actively rendered (Q/E to switch view, all generate)
    int activeLayerIdx = 0;

    // --- Main Loop ---
    bool running = true;
    Uint64 lastTicks = SDL_GetTicks();
    SDL_Log("[PoC] Running. WASD=scroll, Q/E=swap layer view, +/-=zoom, ESC=quit");
    SDL_Log("[PoC] %d layers loaded, streaming generation active.", (int)layers.size());

    while (running) {
        // --- Delta time ---
        Uint64 currentTicks = SDL_GetTicks();
        float deltaTime = static_cast<float>(currentTicks - lastTicks) / 1000.0f;
        lastTicks = currentTicks;
        if (deltaTime > 0.1f) deltaTime = 0.1f;

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
                else if (event.key.scancode == SDL_SCANCODE_Q && !event.key.repeat) {
                    if (layers.size() > 1) {
                        activeLayerIdx = (activeLayerIdx + static_cast<int>(layers.size()) - 1) % static_cast<int>(layers.size());
                        SDL_Log("[PoC] Viewing layer %d: '%s'", activeLayerIdx, layers[activeLayerIdx].def.name.c_str());
                    }
                }
                else if (event.key.scancode == SDL_SCANCODE_E && !event.key.repeat) {
                    if (layers.size() > 1) {
                        activeLayerIdx = (activeLayerIdx + 1) % static_cast<int>(layers.size());
                        SDL_Log("[PoC] Viewing layer %d: '%s'", activeLayerIdx, layers[activeLayerIdx].def.name.c_str());
                    }
                }
                else if ((event.key.scancode == SDL_SCANCODE_EQUALS || event.key.scancode == SDL_SCANCODE_KP_PLUS) && !event.key.repeat) {
                    zoomLevel += ZOOM_STEP;
                    if (zoomLevel > ZOOM_MAX) zoomLevel = ZOOM_MAX;
                }
                else if ((event.key.scancode == SDL_SCANCODE_MINUS || event.key.scancode == SDL_SCANCODE_KP_MINUS) && !event.key.repeat) {
                    zoomLevel -= ZOOM_STEP;
                    if (zoomLevel < ZOOM_MIN) zoomLevel = ZOOM_MIN;
                }
            }
            else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                if (event.wheel.y > 0) { zoomLevel += ZOOM_STEP; if (zoomLevel > ZOOM_MAX) zoomLevel = ZOOM_MAX; }
                else if (event.wheel.y < 0) { zoomLevel -= ZOOM_STEP; if (zoomLevel < ZOOM_MIN) zoomLevel = ZOOM_MIN; }
            }
        }

        // --- Camera Update ---
        const bool* keyState = SDL_GetKeyboardState(NULL);
        camera.Update(deltaTime, cfg.scroll_speed, keyState);

        // --- Streaming generation: fill visible cells for ALL layers ---
        {
            // Compute visible world area from camera
            float camX = camera.GetX();
            float camY = camera.GetY();
            float viewW = static_cast<float>(cfg.viewport_width) / zoomLevel;
            float viewH = static_cast<float>(cfg.viewport_height) / zoomLevel;
            float worldLeft = camX - viewW * camera.GetPivotX() - MARGIN;
            float worldTop = camY - viewH * camera.GetPivotY() - MARGIN;
            float worldRight = worldLeft + viewW + MARGIN * 2.0f;
            float worldBottom = worldTop + viewH + MARGIN * 2.0f;

            int budget = TILES_PER_FRAME;

            for (LayerState& layer : layers) {
                if (budget <= 0) break;
                if (layer.def.tiles.empty()) continue;

                float cw = layer.cellW;
                float ch = layer.cellH;
                int numTiles = static_cast<int>(layer.def.tiles.size());

                // Grid cell range that covers the visible area
                int colMin = static_cast<int>(std::floor(worldLeft / cw));
                int colMax = static_cast<int>(std::ceil(worldRight / cw));
                int rowMin = static_cast<int>(std::floor(worldTop / ch));
                int rowMax = static_cast<int>(std::ceil(worldBottom / ch));

                for (int row = rowMin; row <= rowMax && budget > 0; ++row) {
                    for (int col = colMin; col <= colMax && budget > 0; ++col) {
                        auto key = std::make_pair(col, row);
                        if (layer.generatedCells.count(key) > 0) continue;

                        // Mark as generated (even if we leave a gap)
                        layer.generatedCells.insert(key);

                        // Find candidates based on placed neighbors
                        float px = static_cast<float>(col) * cw;
                        float py = static_cast<float>(row) * ch;

                        // Check placed neighbors
                        std::vector<int> candidates;
                        for (int t = 0; t < numTiles; ++t) {
                            const TileDef& cand = layer.def.tiles[t];
                            bool valid = true;

                            // Check left neighbor
                            auto leftKey = std::make_pair(col - 1, row);
                            if (layer.generatedCells.count(leftKey) > 0) {
                                const PlacedTile* leftTile = layer.map.QueryPoint(px - cw * 0.5f, py + ch * 0.5f);
                                if (leftTile) {
                                    auto it = layer.def.idIndex.find(leftTile->tileId);
                                    if (it != layer.def.idIndex.end()) {
                                        const TileDef& nb = layer.def.tiles[it->second];
                                        if (!nb.adjacency.right.empty()) {
                                            bool found = false;
                                            for (const auto& id : nb.adjacency.right) { if (id == cand.id) { found = true; break; } }
                                            if (!found) valid = false;
                                        }
                                        if (valid && !cand.adjacency.left.empty()) {
                                            bool found = false;
                                            for (const auto& id : cand.adjacency.left) { if (id == nb.id) { found = true; break; } }
                                            if (!found) valid = false;
                                        }
                                    }
                                }
                            }

                            // Check top neighbor
                            if (valid) {
                                auto topKey = std::make_pair(col, row - 1);
                                if (layer.generatedCells.count(topKey) > 0) {
                                    const PlacedTile* topTile = layer.map.QueryPoint(px + cw * 0.5f, py - ch * 0.5f);
                                    if (topTile) {
                                        auto it = layer.def.idIndex.find(topTile->tileId);
                                        if (it != layer.def.idIndex.end()) {
                                            const TileDef& nb = layer.def.tiles[it->second];
                                            if (!nb.adjacency.down.empty()) {
                                                bool found = false;
                                                for (const auto& id : nb.adjacency.down) { if (id == cand.id) { found = true; break; } }
                                                if (!found) valid = false;
                                            }
                                            if (valid && !cand.adjacency.up.empty()) {
                                                bool found = false;
                                                for (const auto& id : cand.adjacency.up) { if (id == nb.id) { found = true; break; } }
                                                if (!found) valid = false;
                                            }
                                        }
                                    }
                                }
                            }

                            if (valid) candidates.push_back(t);
                        }

                        // Place a random valid tile (or leave gap)
                        if (!candidates.empty()) {
                            std::uniform_int_distribution<int> dist(0, static_cast<int>(candidates.size()) - 1);
                            const TileDef& chosen = layer.def.tiles[candidates[dist(layer.rng)]];
                            PlacedTile pt;
                            pt.tileId = chosen.id;
                            pt.x = px;
                            pt.y = py;
                            pt.w = cw;
                            pt.h = ch;
                            layer.map.AddTile(pt);
                        }
                        --budget;
                    }
                }
            }
        }

        // --- Render ---
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);

        Uint32 elapsed_ms = static_cast<Uint32>(currentTicks);

        // Render the active layer
        if (!layers.empty()) {
            LayerState& activeLayer = layers[activeLayerIdx];

            MapLayerConfig jigsawCfg;
            jigsawCfg.z_depth = 0;
            jigsawCfg.alpha = 255;
            jigsawCfg.pivot_x = camera.GetPivotX();
            jigsawCfg.pivot_y = camera.GetPivotY();
            jigsawCfg.offset_x = 0.0f;
            jigsawCfg.offset_y = 0.0f;
            jigsawCfg.scale = zoomLevel;
            jigsawCfg.sampling = SamplingMode::Nearest;

            tileRenderer.RenderJigsawLayer(
                renderer, *activeLayer.tileset, activeLayer.map, viewport, camera, jigsawCfg, elapsed_ms);
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
