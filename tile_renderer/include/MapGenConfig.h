#pragma once

#include <string>
#include <vector>

// Defines which tilesets are allowed for a single layer in random map generation.
struct MapGenLayerConfig {
    std::string folder;                        // tileset folder name (e.g. "forest")
    std::vector<std::string> allowedTilesets;  // specific JSON basenames; empty = all in folder

    MapGenLayerConfig() {}
};

// Top-level random map generation configuration.
// Loaded from JSON files in assets/mapgen_configs/.
struct MapGenConfigData {
    std::string name;                          // display name for the config
    unsigned int seed;                         // 0 = use random device
    int width;                                 // map width in cells (for offline gen)
    int height;                                // map height in cells (for offline gen)
    std::vector<MapGenLayerConfig> layers;     // per-layer tileset configuration

    MapGenConfigData() : seed(0), width(32), height(32) {}
};

// Loads and saves MapGenConfig JSON files.
class MapGenConfig {
public:
    // Load from file. Returns true on success.
    bool Load(const std::string& filepath);

    // Save to file. Returns true on success.
    bool Save(const std::string& filepath) const;

    // Parse from a JSON string. Returns true on success.
    bool ParseFromString(const std::string& jsonStr);

    // Serialize to pretty-printed JSON string.
    std::string Serialize() const;

    const MapGenConfigData& Get() const { return m_data; }
    MapGenConfigData& GetMutable() { return m_data; }

private:
    MapGenConfigData m_data;
};
