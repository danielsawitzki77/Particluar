#include "TileWorld.h"
#include <SDL3/SDL.h>
#include <windows.h>
#include <string>
#include <vector>

// Discover all JSON sidecar files recursively under a root directory.
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
                if (name.size() > 5 && name.substr(name.size() - 5) == ".json") {
                    std::string baseName = name.substr(0, name.size() - 5);
                    std::string pngPath = dir + "\\" + baseName + ".png";
                    DWORD attr = GetFileAttributesA(pngPath.c_str());
                    if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
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

static TilesetDef BuildTilesetDef(const Tileset& ts)
{
    TilesetDef def;
    def.name = ts.name;
    def.textureWidth = ts.textureWidth;
    def.textureHeight = ts.textureHeight;
    def.sheetScale = ts.sheetScale;
    def.tiles = ts.tiles;
    def.idIndex = ts.idIndex;
    return def;
}

int TileWorld::LoadTilesets(SDL_Renderer* renderer, const std::string& rootDir)
{
    m_layers.clear();
    m_activeLayer = 0;

    TilesetLoader loader;
    std::vector<std::string> jsonPaths = FindAllTilesetJsons(rootDir);
    SDL_Log("[TileWorld] Found %d tileset JSON(s) in %s", (int)jsonPaths.size(), rootDir.c_str());

    for (const std::string& jsonPath : jsonPaths) {
        Layer layer;
        if (loader.LoadTilesetFromJson(renderer, jsonPath, layer.tileset)) {
            layer.def = BuildTilesetDef(layer.tileset);
            layer.generator.Init(layer.def);
            SDL_Log("[TileWorld]   Loaded: %s (%d tiles)", jsonPath.c_str(), (int)layer.tileset.tiles.size());
            m_layers.push_back(std::move(layer));
        } else {
            SDL_Log("[TileWorld]   Failed: %s", jsonPath.c_str());
        }
    }

    return static_cast<int>(m_layers.size());
}

void TileWorld::Update(const Camera& camera, const Viewport& viewport,
                       float zoomLevel, int budget, float margin)
{
    if (m_layers.empty()) return;

    const ViewportRect& vp = viewport.GetRect();
    float viewW = static_cast<float>(vp.width) / zoomLevel;
    float viewH = static_cast<float>(vp.height) / zoomLevel;
    float camX = camera.GetX();
    float camY = camera.GetY();
    float worldLeft = camX - viewW * camera.GetPivotX() - margin;
    float worldTop = camY - viewH * camera.GetPivotY() - margin;
    float worldRight = worldLeft + viewW + margin * 2.0f;
    float worldBottom = worldTop + viewH + margin * 2.0f;

    for (Layer& layer : m_layers) {
        if (budget <= 0) break;
        int used = layer.generator.Generate(
            worldLeft, worldTop, worldRight, worldBottom,
            camX, camY, budget);
        budget -= used;
    }
}

void TileWorld::Render(SDL_Renderer* renderer, const Camera& camera, const Viewport& viewport,
                       float zoomLevel, Uint32 elapsedMs)
{
    if (m_layers.empty() || m_activeLayer < 0 || m_activeLayer >= static_cast<int>(m_layers.size()))
        return;

    Layer& layer = m_layers[m_activeLayer];

    MapLayerConfig cfg;
    cfg.zDepth = 0;
    cfg.alpha = 255;
    cfg.pivotX = camera.GetPivotX();
    cfg.pivotY = camera.GetPivotY();
    cfg.offsetX = 0.0f;
    cfg.offsetY = 0.0f;
    cfg.scale = zoomLevel;
    cfg.sampling = SamplingMode::Nearest;

    m_tileRenderer.RenderJigsawLayer(
        renderer, layer.tileset, layer.generator.GetMap(), viewport, camera, cfg, elapsedMs);
}

void TileWorld::SetActiveLayer(int idx)
{
    if (idx >= 0 && idx < static_cast<int>(m_layers.size()))
        m_activeLayer = idx;
}

void TileWorld::NextLayer()
{
    if (m_layers.size() > 1)
        m_activeLayer = (m_activeLayer + 1) % static_cast<int>(m_layers.size());
}

void TileWorld::PrevLayer()
{
    if (m_layers.size() > 1)
        m_activeLayer = (m_activeLayer + static_cast<int>(m_layers.size()) - 1) % static_cast<int>(m_layers.size());
}

const std::string& TileWorld::GetActiveLayerName() const
{
    static const std::string empty;
    if (m_layers.empty()) return empty;
    return m_layers[m_activeLayer].def.name;
}
