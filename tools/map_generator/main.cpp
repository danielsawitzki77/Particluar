// MapGenerator — Offline CLI Map Generation Tool
// Generates map files using the same adjacency-constrained tile placement
// algorithm as the realtime streaming generator (TilePlacementSolver).
//
// Usage: MapGenerator.exe --tileset <folder> --output <path> --width <int> --height <int> [--seed <int>]

#include "TilePlacementSolver.h"
#include "TilesetLoader.h"
#include "MapLoader.h"
#include <SDL3/SDL.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <random>

static void PrintUsage(const char* progName)
{
    std::cerr << "Usage: " << progName
              << " --tileset <folder> --output <path> --width <int> --height <int> [--seed <int>]"
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

int main(int argc, char* argv[])
{
    std::string tilesetPath;
    std::string outputPath;
    int width = 0;
    int height = 0;
    unsigned int seed = 0;
    bool hasSeed = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--tileset") == 0 && i + 1 < argc) {
            tilesetPath = argv[++i];
        } else if (std::strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            outputPath = argv[++i];
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

    if (tilesetPath.empty() || outputPath.empty() || width <= 0 || height <= 0) {
        PrintUsage(argv[0]);
        return 1;
    }
    if (width > 4096 || height > 4096) {
        std::cerr << "Error: dimensions must not exceed 4096." << std::endl;
        return 1;
    }

    // Load tileset (data-only, no renderer needed)
    TilesetLoader loader;
    TilesetDef tileset;
    if (!loader.LoadTilesetDef(tilesetPath, tileset)) {
        std::cerr << "Error: Failed to load tileset from: " << tilesetPath << std::endl;
        return 1;
    }
    if (tileset.tiles.empty()) {
        std::cerr << "Error: Tileset contains no valid tiles." << std::endl;
        return 1;
    }

    // Initialize solver (same algorithm as streaming generator)
    TilePlacementSolver solver;
    solver.Init(tileset);

    // Seed RNG
    std::mt19937 rng;
    if (hasSeed && seed != 0) {
        rng.seed(seed);
    } else {
        std::random_device rd;
        rng.seed(rd());
    }

    // Generate grid: row-by-row, using solver for adjacency-constrained selection
    // Grid stores tile index per cell (-1 = gap)
    std::vector<std::vector<int>> grid(height, std::vector<int>(width, -1));

    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            int leftIdx = (col > 0) ? grid[row][col - 1] : -1;
            int topIdx = (row > 0) ? grid[row - 1][col] : -1;
            grid[row][col] = solver.PickRandom(leftIdx, topIdx, rng);
        }
    }

    // Build MapData for output
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
            // else empty string = gap
        }
    }

    // Save map
    MapLoader mapLoader;
    if (!mapLoader.SaveMap(outputPath, mapData)) {
        std::cerr << "Error: Failed to write map file to: " << outputPath << std::endl;
        return 1;
    }

    std::cout << "Map generated: " << outputPath
              << " (" << width << "x" << height << ", " << placed << " tiles placed, "
              << (width * height - placed) << " gaps)" << std::endl;
    return 0;
}
