#pragma once

#include "JigsawMap.h"
#include "TilesetLoader.h"
#include "WFCGenerator.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

// Grid coordinate identifying a chunk in the infinite map.
struct ChunkCoord {
    int cx, cy;
};

// Manages on-demand generation for infinite jigsaw maps.
// The world is divided into square chunks of configurable pixel size (default 512).
// Chunks are generated lazily via WFC when first queried, and cached for future queries.
class ChunkManager {
public:
    void SetChunkSize(int pixels);   // default 512
    int GetChunkSize() const;

    // Generate chunks covering the given world-space rect.
    // For each chunk that overlaps the rect and hasn't been generated yet,
    // runs WFC jigsaw generation using the tileset and a deterministic per-chunk seed.
    void EnsureGenerated(float wx, float wy, float ww, float wh,
                         const TilesetDef& tileset, unsigned int baseSeed);

    // Query placed tiles in world-space rect (aggregates results from all overlapping chunks).
    std::vector<const PlacedTile*> QueryRect(float qx, float qy, float qw, float qh) const;

    bool IsChunkGenerated(int cx, int cy) const;

private:
    int m_chunkSize = 512;
    std::unordered_map<int64_t, JigsawMap> m_chunks;

    int64_t PackKey(int cx, int cy) const;
    ChunkCoord ToChunkCoord(float wx, float wy) const;
};
