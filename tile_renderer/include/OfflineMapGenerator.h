#pragma once

#include "TilePlacementSolver.h"
#include "TilesetLoader.h"
#include "MapLoader.h"
#include "MapGenConfig.h"

#include <string>
#include <vector>
#include <random>

// Offline map generation using the same adjacency-constrained tile placement
// algorithm as the realtime streaming generator (TilePlacementSolver).
// Produces complete MapData files for a given tileset and dimensions.
class OfflineMapGenerator {
public:
    // Generate a single-layer map from a tileset folder.
    // Returns true on success; the map is written to outputPath.
    bool GenerateLayer(const std::string& tilesetFolder,
                       const std::vector<std::string>& allowedTilesets,
                       const std::vector<SubmapRef>& submaps,
                       int width, int height,
                       unsigned int seed, const std::string& outputPath);

    // Generate a full multi-layer map from a config file.
    // Returns true if all layers generated successfully.
    bool GenerateFromConfig(const MapGenConfigData& config,
                            int widthOverride, int heightOverride,
                            unsigned int seedOverride, bool hasSeedOverride,
                            const std::string& outputPath);

private:
    void StampSubmaps(std::vector<std::vector<int>>& grid,
                      std::vector<std::vector<std::string>>& stampedIds,
                      const std::vector<SubmapRef>& submaps,
                      int width, int height, std::mt19937& rng);
};
