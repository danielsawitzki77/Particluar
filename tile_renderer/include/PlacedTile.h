#pragma once

#include <string>
#include <vector>

// Represents a tile placed at an absolute pixel position in a JigsawMap.
struct PlacedTile {
    std::string tileId;  // references TileDef::id in tileset
    float x, y;           // absolute pixel position (top-left corner)
    float w, h;           // effective rendered size (pixels)
    std::vector<std::string> labels; // map-level labels (additive to tile def labels)
    bool blocked;         // per-tile blocking flag (map-level override)

    PlacedTile() : x(0), y(0), w(0), h(0), blocked(false) {}
};
