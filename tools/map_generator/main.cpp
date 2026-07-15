// MapGenerator — Offline CLI Map Generation Tool
// Thin CLI wrapper around OfflineMapGenerator (TileRenderer library).
//
// Usage:
//   MapGenerator.exe --tileset <folder> --output <path> --width <int> --height <int> [--seed <int>]
//   MapGenerator.exe --config <config.json> --output <path> [--seed <int>]

#include "OfflineMapGenerator.h"
#include "MapGenConfig.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

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

    OfflineMapGenerator generator;

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

        bool ok = generator.GenerateFromConfig(config.Get(), width, height, seed, hasSeed, outputPath);
        return ok ? 0 : 1;
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
    std::vector<SubmapRef> noSubmaps;
    bool ok = generator.GenerateLayer(tilesetPath, noFilter, noSubmaps, width, height, effectiveSeed, outputPath);
    return ok ? 0 : 1;
}
