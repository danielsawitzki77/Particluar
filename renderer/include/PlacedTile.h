#pragma once

#include <string>

// Represents a tile placed at an absolute pixel position in a JigsawMap.
struct PlacedTile {
    std::string tile_id;  // references TileDef::id in tileset
    float x, y;           // absolute pixel position (top-left corner)
    float w, h;           // effective rendered size (pixels)
};
