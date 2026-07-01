#include "ChunkManager.h"
#include <cmath>
#include <algorithm>

void ChunkManager::SetChunkSize(int pixels)
{
    if (pixels > 0) {
        m_chunkSize = pixels;
    }
}

int ChunkManager::GetChunkSize() const
{
    return m_chunkSize;
}

int64_t ChunkManager::PackKey(int cx, int cy) const
{
    return (static_cast<int64_t>(cx) << 32) | (static_cast<int64_t>(cy) & 0xFFFFFFFF);
}

ChunkCoord ChunkManager::ToChunkCoord(float wx, float wy) const
{
    ChunkCoord cc;
    cc.cx = static_cast<int>(std::floor(wx / static_cast<float>(m_chunkSize)));
    cc.cy = static_cast<int>(std::floor(wy / static_cast<float>(m_chunkSize)));
    return cc;
}

bool ChunkManager::IsChunkGenerated(int cx, int cy) const
{
    int64_t key = PackKey(cx, cy);
    return m_chunks.find(key) != m_chunks.end();
}

void ChunkManager::EnsureGenerated(float wx, float wy, float ww, float wh,
                                   const TilesetDef& tileset, unsigned int baseSeed)
{
    // Determine the range of chunks that overlap the query rect.
    ChunkCoord topLeft = ToChunkCoord(wx, wy);
    ChunkCoord bottomRight = ToChunkCoord(wx + ww - 0.001f, wy + wh - 0.001f);

    WFCGenerator generator;

    for (int cy = topLeft.cy; cy <= bottomRight.cy; ++cy) {
        for (int cx = topLeft.cx; cx <= bottomRight.cx; ++cx) {
            int64_t key = PackKey(cx, cy);
            if (m_chunks.find(key) != m_chunks.end()) {
                continue; // Already generated
            }

            // Compute the world-space origin for this chunk.
            float origin_x = static_cast<float>(cx) * static_cast<float>(m_chunkSize);
            float origin_y = static_cast<float>(cy) * static_cast<float>(m_chunkSize);

            // Derive a per-chunk seed from baseSeed + chunk coords for determinism.
            // Uses a simple hash combining function.
            unsigned int chunkSeed = baseSeed;
            chunkSeed ^= static_cast<unsigned int>(cx) * 2654435761u;
            chunkSeed ^= static_cast<unsigned int>(cy) * 2246822519u;
            // Ensure seed != 0 (which means non-deterministic in WFC)
            if (chunkSeed == 0) chunkSeed = 1;

            // Set up jigsaw WFC params for this chunk.
            JigsawWFCParams params;
            params.target_width = static_cast<float>(m_chunkSize);
            params.target_height = static_cast<float>(m_chunkSize);
            params.origin_x = origin_x;
            params.origin_y = origin_y;
            params.seed = chunkSeed;
            params.tileset = &tileset;
            params.layer_scale = 1.0f;

            JigsawWFCResult result = generator.GenerateJigsaw(params);

            if (result.status == WFCStatus::Success) {
                m_chunks[key] = result.map;
            } else {
                // On contradiction or failure, retry with adjusted seed.
                params.seed = chunkSeed + 7919u; // prime offset
                result = generator.GenerateJigsaw(params);
                if (result.status == WFCStatus::Success) {
                    m_chunks[key] = result.map;
                } else {
                    // Leave chunk empty and insert an empty JigsawMap to avoid retrying.
                    SDL_Log("[ChunkManager] Chunk (%d,%d) generation failed. Leaving empty.", cx, cy);
                    m_chunks[key] = JigsawMap();
                }
            }
        }
    }
}

std::vector<const PlacedTile*> ChunkManager::QueryRect(float qx, float qy, float qw, float qh) const
{
    std::vector<const PlacedTile*> results;

    // Determine which chunks overlap the query rect.
    ChunkCoord topLeft = ToChunkCoord(qx, qy);
    ChunkCoord bottomRight = ToChunkCoord(qx + qw - 0.001f, qy + qh - 0.001f);

    for (int cy = topLeft.cy; cy <= bottomRight.cy; ++cy) {
        for (int cx = topLeft.cx; cx <= bottomRight.cx; ++cx) {
            int64_t key = PackKey(cx, cy);
            auto it = m_chunks.find(key);
            if (it == m_chunks.end()) {
                continue; // Chunk not generated yet
            }

            // Query the chunk's JigsawMap with the full query rect.
            // The spatial index will filter to tiles actually intersecting.
            std::vector<const PlacedTile*> chunkTiles = it->second.QueryRect(qx, qy, qw, qh);
            results.insert(results.end(), chunkTiles.begin(), chunkTiles.end());
        }
    }

    return results;
}
