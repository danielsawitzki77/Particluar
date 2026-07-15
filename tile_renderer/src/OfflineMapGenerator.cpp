#include "OfflineMapGenerator.h"

#include <iostream>

bool OfflineMapGenerator::GenerateLayer(const std::string& tilesetFolder,
                                        const std::vector<std::string>& allowedTilesets,
                                        const std::vector<SubmapRef>& submaps,
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

    // Build grid: tile index per cell (-1 = gap, -2 = stamped by submap)
    std::vector<std::vector<int>> grid(height, std::vector<int>(width, -1));
    std::vector<std::vector<std::string>> stampedIds(height, std::vector<std::string>(width));

    // Stamp submaps before main generation
    if (!submaps.empty()) {
        StampSubmaps(grid, stampedIds, submaps, width, height, rng);
    }

    // Main generation: fill non-stamped cells
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            if (grid[row][col] == -2) continue; // skip stamped cells
            int leftIdx = (col > 0 && grid[row][col - 1] >= 0) ? grid[row][col - 1] : -1;
            int topIdx = (row > 0 && grid[row - 1][col] >= 0) ? grid[row - 1][col] : -1;
            grid[row][col] = solver.PickRandom(leftIdx, topIdx, rng);
        }
    }

    // Build MapData output
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
            if (idx == -2) {
                mapData.grid[row][col] = stampedIds[row][col];
                if (!stampedIds[row][col].empty()) ++placed;
            } else if (idx >= 0) {
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

bool OfflineMapGenerator::GenerateFromConfig(const MapGenConfigData& config,
                                             int widthOverride, int heightOverride,
                                             unsigned int seedOverride, bool hasSeedOverride,
                                             const std::string& outputPath)
{
    unsigned int effectiveSeed = hasSeedOverride ? seedOverride : config.seed;
    int effectiveWidth = (widthOverride > 0) ? widthOverride : config.width;
    int effectiveHeight = (heightOverride > 0) ? heightOverride : config.height;

    if (effectiveWidth <= 0 || effectiveHeight <= 0) {
        std::cerr << "Error: dimensions must be positive." << std::endl;
        return false;
    }
    if (effectiveWidth > 4096 || effectiveHeight > 4096) {
        std::cerr << "Error: dimensions must not exceed 4096." << std::endl;
        return false;
    }
    if (config.layers.empty()) {
        std::cerr << "Error: Config has no layers defined." << std::endl;
        return false;
    }

    std::cout << "Using config: " << config.name << " (seed=" << effectiveSeed
              << ", " << effectiveWidth << "x" << effectiveHeight
              << ", " << config.layers.size() << " layer(s))" << std::endl;

    bool allOk = true;
    for (size_t i = 0; i < config.layers.size(); ++i) {
        const MapGenLayerConfig& layerCfg = config.layers[i];
        std::string resolvedPath = "assets/tilesets/" + layerCfg.folder;

        // Build layer output filename
        std::string layerOutput = outputPath;
        if (config.layers.size() > 1) {
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
                           config.submaps,
                           effectiveWidth, effectiveHeight,
                           effectiveSeed, layerOutput)) {
            allOk = false;
        }
    }

    return allOk;
}

void OfflineMapGenerator::StampSubmaps(std::vector<std::vector<int>>& grid,
                                       std::vector<std::vector<std::string>>& stampedIds,
                                       const std::vector<SubmapRef>& submaps,
                                       int width, int height, std::mt19937& rng)
{
    MapLoader mapLoader;

    struct LoadedSubmap {
        MapData data;
        int chance;
    };
    std::vector<LoadedSubmap> loadedSubmaps;
    int totalSubmapChance = 0;

    for (const SubmapRef& ref : submaps) {
        std::string mapPath = "assets/maps/" + ref.mapFile;
        MapData smData;
        if (mapLoader.LoadMap(mapPath, smData)) {
            loadedSubmaps.push_back({ std::move(smData), ref.chance });
            totalSubmapChance += ref.chance;
        } else {
            std::cerr << "Warning: Failed to load submap: " << mapPath << std::endl;
        }
    }

    if (loadedSubmaps.empty() || totalSubmapChance <= 0) return;

    int totalArea = width * height;
    int submapAttempts = totalArea / 50;
    if (submapAttempts < 1) submapAttempts = 1;

    std::uniform_int_distribution<int> chanceDist(0, totalSubmapChance - 1);
    std::uniform_int_distribution<int> colDist(0, width - 1);
    std::uniform_int_distribution<int> rowDist(0, height - 1);

    for (int attempt = 0; attempt < submapAttempts; ++attempt) {
        int roll = chanceDist(rng);
        int cumulative = 0;
        const LoadedSubmap* picked = nullptr;
        for (const LoadedSubmap& sm : loadedSubmaps) {
            cumulative += sm.chance;
            if (roll < cumulative) {
                picked = &sm;
                break;
            }
        }
        if (!picked) continue;

        const MapData& smData = picked->data;
        if (smData.width <= 0 || smData.height <= 0) continue;
        if (smData.width > width || smData.height > height) continue;

        int maxCol = width - smData.width;
        int maxRow = height - smData.height;
        std::uniform_int_distribution<int> placeCDist(0, maxCol);
        std::uniform_int_distribution<int> placeRDist(0, maxRow);
        int startCol = placeCDist(rng);
        int startRow = placeRDist(rng);

        // Check no cells are already stamped
        bool canPlace = true;
        for (int r = 0; r < smData.height && canPlace; ++r) {
            for (int c = 0; c < smData.width && canPlace; ++c) {
                if (grid[startRow + r][startCol + c] == -2) {
                    canPlace = false;
                }
            }
        }
        if (!canPlace) continue;

        // Stamp
        for (int r = 0; r < smData.height; ++r) {
            for (int c = 0; c < smData.width; ++c) {
                grid[startRow + r][startCol + c] = -2;
                if (r < static_cast<int>(smData.grid.size()) &&
                    c < static_cast<int>(smData.grid[r].size())) {
                    stampedIds[startRow + r][startCol + c] = smData.grid[r][c];
                }
            }
        }
    }
}
