// MapGenerator — Offline CLI Map Generation Tool
// Generates map files using the same adjacency-constrained tile placement
// algorithm as the realtime streaming generator (TilePlacementSolver).
//
// Usage:
//   MapGenerator.exe --tileset <folder> --output <path> --width <int> --height <int> [--seed <int>]
//   MapGenerator.exe --config <config.json> --output <path> [--seed <int>]

#include "TilePlacementSolver.h"
#include "TilesetLoader.h"
#include "MapLoader.h"
#include "MapGenConfig.h"
#include <SDL3/SDL.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <random>

static void PrintUsage(const char* progName)
{
    std::cerr << "Usage:" << std::endl;
    std::cerr << "  " << progName
              << " --tileset <folder> --output <path> --width <int> --height <int> [--seed <int>]"
              << std::endl;
    std::cerr << "  " << progName
              << " --config <config.json> --output <path> [--seed <int>]"
              << std::endl;
}

static bool ParseInt(const char* str, int& out)
{
    char* end = nullptr;
    long val = std::strtol(str, &end, 10);
    if (end == str || *end != '\0') return false;
    out = static_cast<int>(val);
    return true;
}

// Generate a single layer map with given tileset and dimensions.
static bool GenerateLayer(const std::string& tilesetFolder,
                          const std::vector<std::string>& allowedTilesets,
                          int width, int height,
                          unsigned int seed, const std::string& outputPath)
{
    TilesetLoader loader;
    TilesetDef tileset;
    bool loaded = false;

    // If specific tilesets are listed, try loading each one by JSON path
    if (!allowedTilesets.empty()) {
        for (const std::string& tsName : allowedTilesets) {
            std::string jsonPath = tilesetFolder + "/" + tsName + ".json";
            if (loader.LoadTilesetDefFromJson(jsonPath, tileset) && !tileset.tiles.empty()) {
                loaded = true;
                break;
            }
        }
    }

    // Fallback: load from folder (uses folder name convention)
    if (!loaded) {
        loaded = loader.LoadTilesetDef(tilesetFolder, tileset);
    }

    if (!loaded) {
        std::cerr << "Error: Failed to load tileset from: " << tilesetFolder << std::endl;
        return false;
    }
    if (tileset.tiles.empty()) {
        std::cerr << "Error: Tileset contains no valid tiles." << std::endl;
        return false;
    }

    TilePlacementSolver solver;
    solver.Init(tileset);

    std::mt19937 rng;
    if (seed != 0) {
        rng.seed(seed);
    } else {
        std::random_device rd;
        rng.seed(rd());
    }

    std::vector<std::vector<int>> grid(height, std::vector<int>(width, -1));

    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            int leftIdx = (col > 0) ? grid[row][col - 1] : -1;
            int topIdx = (row > 0) ? grid[row - 1][col] : -1;
            grid[row][col] = solver.PickRandom(leftIdx, topIdx, rng);
        }
    }

    MapData mapData;
    mapData.width = width;
    mapData.height = height;
    mapData.tilesetId = tileset.name;
    mapData.grid.resize(height);

    int placed = 0;
    for (int row = 0; row < height; ++row) {
        mapData.grid[row].resize(width);
        for (int col = 0; col < width; ++col) {
            int idx = grid[row][col];
            if (idx >= 0) {
                mapData.grid[row][col] = tileset.tiles[idx].id;
                ++placed;
            }
        }
    }

    MapLoader mapLoader;
    if (!mapLoader.SaveMap(outputPath, mapData)) {
        std::cerr << "Error: Failed to write map file to: " << outputPath << std::endl;
        return false;
    }

    std::cout << "Map generated: " << outputPath
              << " (" << width << "x" << height << ", " << placed << " tiles placed, "
              << (width * height - placed) << " gaps)" << std::endl;
    return true;
}

int main(int argc, char* argv[])
{
    std::string tilesetPath;
    std::string outputPath;
    std::string configPath;
    int width = 0;
    int height = 0;
    unsigned int seed = 0;
    bool hasSeed = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--tileset") == 0 && i + 1 < argc) {
            tilesetPath = argv[++i];
        } else if (std::strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            outputPath = argv[++i];
        } else if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            configPath = argv[++i];
        } else if (std::strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            if (!ParseInt(argv[++i], width)) {
                std::cerr << "Error: invalid --width value." << std::endl;
                return 1;
            }
        } else if (std::strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            if (!ParseInt(argv[++i], height)) {
                std::cerr << "Error: invalid --height value." << std::endl;
                return 1;
            }
        } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            int s = 0;
            if (!ParseInt(argv[++i], s)) {
                std::cerr << "Error: invalid --seed value." << std::endl;
                return 1;
            }
            seed = static_cast<unsigned int>(s);
            hasSeed = true;
        } else {
            std::cerr << "Error: Unknown argument: " << argv[i] << std::endl;
            PrintUsage(argv[0]);
            return 1;
        }
    }

    // Config-based generation
    if (!configPath.empty()) {
        if (outputPath.empty()) {
            std::cerr << "Error: --output is required with --config." << std::endl;
            PrintUsage(argv[0]);
            return 1;
        }

        MapGenConfig config;
        if (!config.Load(configPath)) {
            std::cerr << "Error: Failed to load config from: " << configPath << std::endl;
            return 1;
        }

        const MapGenConfigData& cfgData = config.Get();

        // CLI --seed overrides config seed
        unsigned int effectiveSeed = hasSeed ? seed : cfgData.seed;
        int effectiveWidth = (width > 0) ? width : cfgData.width;
        int effectiveHeight = (height > 0) ? height : cfgData.height;

        if (effectiveWidth <= 0 || effectiveHeight <= 0) {
            std::cerr << "Error: dimensions must be positive." << std::endl;
            return 1;
        }
        if (effectiveWidth > 4096 || effectiveHeight > 4096) {
            std::cerr << "Error: dimensions must not exceed 4096." << std::endl;
            return 1;
        }

        if (cfgData.layers.empty()) {
            std::cerr << "Error: Config has no layers defined." << std::endl;
            return 1;
        }

        std::cout << "Using config: " << cfgData.name << " (seed=" << effectiveSeed
                  << ", " << effectiveWidth << "x" << effectiveHeight
                  << ", " << cfgData.layers.size() << " layer(s))" << std::endl;

        // Generate one map per layer, naming with layer index suffix
        bool allOk = true;
        for (size_t i = 0; i < cfgData.layers.size(); ++i) {
            const MapGenLayerConfig& layerCfg = cfgData.layers[i];

            // Resolve tileset path from folder + first allowed tileset
            std::string resolvedPath = "assets/tilesets/" + layerCfg.folder;

            // Build layer output filename
            std::string layerOutput = outputPath;
            if (cfgData.layers.size() > 1) {
                // Insert layer index before .json extension
                size_t dotPos = layerOutput.rfind('.');
                if (dotPos != std::string::npos) {
                    layerOutput = layerOutput.substr(0, dotPos) + "_layer"
                                + std::to_string(i) + layerOutput.substr(dotPos);
                } else {
                    layerOutput += "_layer" + std::to_string(i);
                }
            }

            std::cout << "  Layer " << i << ": folder=" << layerCfg.folder;
            if (!layerCfg.allowedTilesets.empty()) {
                std::cout << " allowed=[";
                for (size_t j = 0; j < layerCfg.allowedTilesets.size(); ++j) {
                    if (j > 0) std::cout << ", ";
                    std::cout << layerCfg.allowedTilesets[j];
                }
                std::cout << "]";
            }
            std::cout << std::endl;

            if (!GenerateLayer(resolvedPath, layerCfg.allowedTilesets,
                               effectiveWidth, effectiveHeight,
                               effectiveSeed, layerOutput)) {
                allOk = false;
            }
        }

        return allOk ? 0 : 1;
    }

    // Legacy single-tileset mode
    if (tilesetPath.empty() || outputPath.empty() || width <= 0 || height <= 0) {
        PrintUsage(argv[0]);
        return 1;
    }
    if (width > 4096 || height > 4096) {
        std::cerr << "Error: dimensions must not exceed 4096." << std::endl;
        return 1;
    }

    unsigned int effectiveSeed = (hasSeed && seed != 0) ? seed : 0;
    std::vector<std::string> noFilter;
    return GenerateLayer(tilesetPath, noFilter, width, height, effectiveSeed, outputPath) ? 0 : 1;
}
