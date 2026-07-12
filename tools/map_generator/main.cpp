// mapgen - Offline CLI Map Generator Tool
// Generates map files using WFC (Wave Function Collapse) algorithm.
// Usage: mapgen.exe --tileset <path> --output <path> --width <int> --height <int> [--seed <int>]

#include "TilesetLoader.h"
#include "WFCGenerator.h"
#include "MapLoader.h"
#include <SDL3/SDL.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

static void PrintUsage(const char* progName)
{
    std::cerr << "Usage: " << progName
              << " --tileset <path> --output <path> --width <int> --height <int> [--seed <int>]"
              << std::endl;
}

static bool ParseInt(const char* str, int& out)
{
    char* end = nullptr;
    long val = std::strtol(str, &end, 10);
    if (end == str || *end != '\0') {
        return false;
    }
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

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--tileset") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "Error: --tileset requires a path argument." << std::endl;
                PrintUsage(argv[0]);
                return 1;
            }
            tilesetPath = argv[++i];
        }
        else if (std::strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "Error: --output requires a path argument." << std::endl;
                PrintUsage(argv[0]);
                return 1;
            }
            outputPath = argv[++i];
        }
        else if (std::strcmp(argv[i], "--width") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "Error: --width requires an integer argument." << std::endl;
                PrintUsage(argv[0]);
                return 1;
            }
            ++i;
            if (!ParseInt(argv[i], width)) {
                std::cerr << "Error: --width value is not a valid integer: " << argv[i] << std::endl;
                return 1;
            }
        }
        else if (std::strcmp(argv[i], "--height") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "Error: --height requires an integer argument." << std::endl;
                PrintUsage(argv[0]);
                return 1;
            }
            ++i;
            if (!ParseInt(argv[i], height)) {
                std::cerr << "Error: --height value is not a valid integer: " << argv[i] << std::endl;
                return 1;
            }
        }
        else if (std::strcmp(argv[i], "--seed") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "Error: --seed requires an integer argument." << std::endl;
                PrintUsage(argv[0]);
                return 1;
            }
            ++i;
            int seedVal = 0;
            if (!ParseInt(argv[i], seedVal)) {
                std::cerr << "Error: --seed value is not a valid integer: " << argv[i] << std::endl;
                return 1;
            }
            seed = static_cast<unsigned int>(seedVal);
            hasSeed = true;
        }
        else {
            std::cerr << "Error: Unknown argument: " << argv[i] << std::endl;
            PrintUsage(argv[0]);
            return 1;
        }
    }

    // Validate required arguments
    if (tilesetPath.empty()) {
        std::cerr << "Error: --tileset is required." << std::endl;
        PrintUsage(argv[0]);
        return 1;
    }
    if (outputPath.empty()) {
        std::cerr << "Error: --output is required." << std::endl;
        PrintUsage(argv[0]);
        return 1;
    }
    if (width <= 0) {
        std::cerr << "Error: --width must be a positive integer." << std::endl;
        PrintUsage(argv[0]);
        return 1;
    }
    if (height <= 0) {
        std::cerr << "Error: --height must be a positive integer." << std::endl;
        PrintUsage(argv[0]);
        return 1;
    }
    if (width > 1024) {
        std::cerr << "Error: --width must not exceed 1024." << std::endl;
        return 1;
    }
    if (height > 1024) {
        std::cerr << "Error: --height must not exceed 1024." << std::endl;
        return 1;
    }

    // Load tileset definition (data-only, no SDL renderer needed)
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

    // Configure WFC parameters
    WFCParams params;
    params.width = width;
    params.height = height;
    params.seed = hasSeed ? seed : 0;  // 0 = non-deterministic
    params.tileset = &tileset;

    // Run WFC generation
    WFCGenerator generator;
    WFCResult result = generator.Generate(params);

    if (result.status == WFCStatus::InvalidInput) {
        std::cerr << "Error: WFC generation failed due to invalid input." << std::endl;
        return 1;
    }
    if (result.status == WFCStatus::Contradiction) {
        std::cerr << "Error: WFC generation encountered a contradiction. "
                  << "The tileset adjacency rules may be too restrictive for the requested dimensions."
                  << std::endl;
        return 1;
    }

    // Save the generated map
    MapLoader mapLoader;
    if (!mapLoader.SaveMap(outputPath, result.map)) {
        std::cerr << "Error: Failed to write map file to: " << outputPath << std::endl;
        return 1;
    }

    std::cout << "Map generated successfully: " << outputPath
              << " (" << width << "x" << height << ")" << std::endl;
    return 0;
}
