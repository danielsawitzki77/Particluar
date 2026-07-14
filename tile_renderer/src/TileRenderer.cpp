#include "TileRenderer.h"
#include <algorithm>
#include <vector>
#include <cstddef>

void TileRenderer::RenderLayer(
    SDL_Renderer* renderer,
    const Tileset& tileset,
    const MapData& mapData,
    const VisibleTileRange& range,
    const Viewport& viewport,
    const Camera& camera,
    int tileWidth, int tileHeight,
    int layer,
    Uint8 alpha,
    Uint32 elapsedMs)
{
    (void)layer; // reserved for future multi-layer support

    // Skip rendering when viewport is invalid
    if (!viewport.IsValid()) {
        return;
    }

    // Apply clip rect to constrain drawing within viewport
    viewport.ApplyClip(renderer);

    const ViewportRect& vp = viewport.GetRect();
    const float camX = camera.GetX();
    const float camY = camera.GetY();
    const float pivotX = camera.GetPivotX();
    const float pivotY = camera.GetPivotY();

    // Screen offset: where the camera world position maps to on screen
    const float screenOriginX = static_cast<float>(vp.x) + pivotX * static_cast<float>(vp.width);
    const float screenOriginY = static_cast<float>(vp.y) + pivotY * static_cast<float>(vp.height);

    const int gridHeight = static_cast<int>(mapData.grid.size());

    // Iterate visible tile range in ascending row/col order (ascending layer order)
    for (int row = range.rowStart; row <= range.rowEnd; ++row) {
        // Skip out-of-bounds rows
        if (row < 0 || row >= gridHeight) {
            continue;
        }

        const int gridWidth = static_cast<int>(mapData.grid[row].size());

        for (int col = range.colStart; col <= range.colEnd; ++col) {
            // Skip out-of-bounds columns
            if (col < 0 || col >= gridWidth) {
                continue;
            }

            const std::string& tileId = mapData.grid[row][col];

            // Compute destination rect — snap to integer pixels to avoid sub-pixel gaps
            float destX = SDL_floorf(screenOriginX + (static_cast<float>(col) * static_cast<float>(tileWidth) - camX));
            float destY = SDL_floorf(screenOriginY + (static_cast<float>(row) * static_cast<float>(tileHeight) - camY));

            // Look up tile ID in tileset
            auto it = tileset.idIndex.find(tileId);
            if (it != tileset.idIndex.end()) {
                // Resolved tile — render texture at native source size (scaled)
                const TileDef& tileDef = tileset.tiles[it->second];
                const SourceRect& src = tileDef.GetCurrentRect(elapsedMs);

                // Destination size = source pixel size * sheetScale * tile_scale
                float finalScale = tileset.sheetScale * tileDef.scale;
                float destW = static_cast<float>(src.w) * finalScale;
                float destH = static_cast<float>(src.h) * finalScale;

                SDL_FRect destRect = { destX, destY, destW, destH };

                SDL_FRect srcRect = {
                    static_cast<float>(src.x),
                    static_cast<float>(src.y),
                    static_cast<float>(src.w),
                    static_cast<float>(src.h)
                };

                // Apply alpha modulation
                SDL_SetTextureAlphaMod(tileset.texture, alpha);

                // Render the tile texture
                SDL_RenderTexture(renderer, tileset.texture, &srcRect, &destRect);
            } else {
                // Unresolved tile ID — render magenta fallback rectangle
                SDL_FRect destRect = {
                    destX, destY,
                    static_cast<float>(tileWidth),
                    static_cast<float>(tileHeight)
                };
                SDL_SetRenderDrawColor(renderer, m_fallbackR, m_fallbackG, m_fallbackB, alpha);
                SDL_RenderFillRect(renderer, &destRect);
            }
        }
    }

    // Remove clip rect after rendering
    viewport.RemoveClip(renderer);
}

void TileRenderer::SetFallbackColor(Uint8 r, Uint8 g, Uint8 b)
{
    m_fallbackR = r;
    m_fallbackG = g;
    m_fallbackB = b;
}

void TileRenderer::SetBackgroundColor(Uint8 r, Uint8 g, Uint8 b)
{
    m_bgR = r;
    m_bgG = g;
    m_bgB = b;
}

void TileRenderer::RenderLayers(
    SDL_Renderer* renderer,
    const std::vector<MapLayer>& layers,
    const Viewport& viewport,
    const Camera& camera,
    int baseTileWidth, int baseTileHeight,
    Uint32 elapsedMs)
{
    if (!viewport.IsValid()) {
        return;
    }

    // Sort layers by zDepth (lower = drawn first / behind).
    // Build index vector to avoid modifying the input.
    std::vector<size_t> sortedIndices(layers.size());
    for (size_t i = 0; i < layers.size(); ++i) {
        sortedIndices[i] = i;
    }
    std::sort(sortedIndices.begin(), sortedIndices.end(),
        [&layers](size_t a, size_t b) {
            return layers[a].GetConfig().zDepth < layers[b].GetConfig().zDepth;
        });

    // Apply clip rect to constrain drawing within viewport
    viewport.ApplyClip(renderer);

    const ViewportRect& vp = viewport.GetRect();

    for (size_t idx = 0; idx < sortedIndices.size(); ++idx) {
        const MapLayer& layer = layers[sortedIndices[idx]];
        const MapLayerConfig& cfg = layer.GetConfig();
        const Tileset* tileset = layer.GetTileset();
        const MapData& mapData = layer.GetMapData();

        // Skip layers with no tileset or no map data
        if (!tileset || mapData.grid.empty()) {
            continue;
        }

        // Skip fully transparent layers
        if (cfg.alpha == 0) {
            continue;
        }

        // Set sampling mode on the texture
        SDL_ScaleMode scaleMode = (cfg.sampling == SamplingMode::Linear)
            ? SDL_SCALEMODE_LINEAR
            : SDL_SCALEMODE_NEAREST;
        SDL_SetTextureScaleMode(tileset->texture, scaleMode);

        // Compute effective tile size for this layer's grid cells
        // The grid cell size is base_tile * layerScale (tiles fill their cell)
        const float layerScale = cfg.scale;
        const float cellW = static_cast<float>(baseTileWidth) * layerScale;
        const float cellH = static_cast<float>(baseTileHeight) * layerScale;

        // Use the layer's own pivot and offset to compute the screen origin
        const float camX = camera.GetX();
        const float camY = camera.GetY();
        const float pivotX = cfg.pivotX;
        const float pivotY = cfg.pivotY;
        const float offsetX = cfg.offsetX;
        const float offsetY = cfg.offsetY;

        const float screenOriginX = static_cast<float>(vp.x) + pivotX * static_cast<float>(vp.width) + offsetX;
        const float screenOriginY = static_cast<float>(vp.y) + pivotY * static_cast<float>(vp.height) + offsetY;

        // Compute visible tile range for this layer using its scaled cell size
        int effectiveTileW = (cellW > 0.0f) ? static_cast<int>(cellW + 0.5f) : baseTileWidth;
        int effectiveTileH = (cellH > 0.0f) ? static_cast<int>(cellH + 0.5f) : baseTileHeight;
        if (effectiveTileW < 1) effectiveTileW = 1;
        if (effectiveTileH < 1) effectiveTileH = 1;

        VisibleTileRange range = viewport.ComputeVisibleTiles(
            camX - offsetX, camY - offsetY,
            pivotX, pivotY,
            effectiveTileW, effectiveTileH);

        const int gridHeight = static_cast<int>(mapData.grid.size());

        for (int row = range.rowStart; row <= range.rowEnd; ++row) {
            if (row < 0 || row >= gridHeight) {
                continue;
            }

            const int gridWidth = static_cast<int>(mapData.grid[row].size());

            for (int col = range.colStart; col <= range.colEnd; ++col) {
                if (col < 0 || col >= gridWidth) {
                    continue;
                }

                const std::string& tileId = mapData.grid[row][col];

                // Compute destination rect (grid cell position)
                float destX = screenOriginX + (static_cast<float>(col) * cellW - camX * layerScale);
                float destY = screenOriginY + (static_cast<float>(row) * cellH - camY * layerScale);

                // Look up tile ID in tileset
                auto it = tileset->idIndex.find(tileId);
                if (it != tileset->idIndex.end()) {
                    const TileDef& tileDef = tileset->tiles[it->second];
                    const SourceRect& src = tileDef.GetCurrentRect(elapsedMs);

                    // Three-level scaling:
                    // final_size = base_tile_size * layerScale * sheetScale * tile_scale
                    float finalScale = layerScale * tileset->sheetScale * tileDef.scale;
                    float finalW = static_cast<float>(baseTileWidth) * finalScale;
                    float finalH = static_cast<float>(baseTileHeight) * finalScale;

                    SDL_FRect destRect = { destX, destY, finalW, finalH };

                    SDL_FRect srcRect = {
                        static_cast<float>(src.x),
                        static_cast<float>(src.y),
                        static_cast<float>(src.w),
                        static_cast<float>(src.h)
                    };

                    // Apply alpha modulation
                    SDL_SetTextureAlphaMod(tileset->texture, cfg.alpha);

                    // Render the tile texture
                    SDL_RenderTexture(renderer, tileset->texture, &srcRect, &destRect);
                } else {
                    // Unresolved tile ID — render magenta fallback rectangle
                    SDL_FRect destRect = { destX, destY, cellW, cellH };
                    SDL_SetRenderDrawColor(renderer, m_fallbackR, m_fallbackG, m_fallbackB, cfg.alpha);
                    SDL_RenderFillRect(renderer, &destRect);
                }
            }
        }
    }

    // Remove clip rect after rendering all layers
    viewport.RemoveClip(renderer);
}

void TileRenderer::RenderJigsawLayer(
    SDL_Renderer* renderer,
    const Tileset& tileset,
    const JigsawMap& map,
    const Viewport& viewport,
    const Camera& camera,
    const MapLayerConfig& config,
    Uint32 elapsedMs)
{
    // Skip rendering when viewport is invalid
    if (!viewport.IsValid()) {
        return;
    }

    // NOTE (Req 9 — Border Overflow): This renderer intentionally does NOT clip
    // tiles to the map boundary. Tiles at edges are allowed to overflow past the
    // boundary. The only clipping applied is the viewport clip rect below, which
    // constrains drawing to the screen region. This preserves visual continuity
    // for tiles placed at the edge of a finite JigsawMap.

    // Set sampling mode on the tileset texture
    if (tileset.texture) {
        SDL_ScaleMode scaleMode = (config.sampling == SamplingMode::Linear)
            ? SDL_SCALEMODE_LINEAR
            : SDL_SCALEMODE_NEAREST;
        SDL_SetTextureScaleMode(tileset.texture, scaleMode);
    }

    // Apply clip rect to constrain drawing within viewport
    viewport.ApplyClip(renderer);

    const ViewportRect& vp = viewport.GetRect();
    const float camX = camera.GetX();
    const float camY = camera.GetY();
    const float pivotX = config.pivotX;
    const float pivotY = config.pivotY;
    const float offsetX = config.offsetX;
    const float offsetY = config.offsetY;
    const float scale = config.scale;

    // Compute the world-space rect visible in the viewport.
    float screenOriginX = static_cast<float>(vp.x) + pivotX * static_cast<float>(vp.width) + offsetX;
    float screenOriginY = static_cast<float>(vp.y) + pivotY * static_cast<float>(vp.height) + offsetY;

    // World-space coordinates at viewport edges
    float worldLeft = camX + (static_cast<float>(vp.x) - screenOriginX) / scale;
    float worldTop = camY + (static_cast<float>(vp.y) - screenOriginY) / scale;
    float worldRight = camX + (static_cast<float>(vp.x + vp.width) - screenOriginX) / scale;
    float worldBottom = camY + (static_cast<float>(vp.y + vp.height) - screenOriginY) / scale;

    float queryW = worldRight - worldLeft;
    float queryH = worldBottom - worldTop;

    // Query the JigsawMap spatial index for tiles in the visible world rect
    std::vector<const PlacedTile*> visibleTiles = map.QueryRect(worldLeft, worldTop, queryW, queryH);

    // Sort by y then x for deterministic render order (Property 14)
    std::sort(visibleTiles.begin(), visibleTiles.end(),
        [](const PlacedTile* a, const PlacedTile* b) {
            if (a->y != b->y) return a->y < b->y;
            return a->x < b->x;
        });

    // Render each visible tile
    for (size_t i = 0; i < visibleTiles.size(); ++i) {
        const PlacedTile& tile = *visibleTiles[i];

        // Compute screen position from world position
        float destX = screenOriginX + (tile.x - camX) * scale;
        float destY = screenOriginY + (tile.y - camY) * scale;
        float destW = tile.w * scale;
        float destH = tile.h * scale;

        SDL_FRect destRect = { destX, destY, destW, destH };

        // Look up tileId in tileset
        auto it = tileset.idIndex.find(tile.tileId);
        if (it != tileset.idIndex.end()) {
            // Resolved tile — render texture with source rect
            const TileDef& tileDef = tileset.tiles[it->second];
            const SourceRect& src = tileDef.GetCurrentRect(elapsedMs);

            SDL_FRect srcRect = {
                static_cast<float>(src.x),
                static_cast<float>(src.y),
                static_cast<float>(src.w),
                static_cast<float>(src.h)
            };

            // Apply alpha modulation
            SDL_SetTextureAlphaMod(tileset.texture, config.alpha);

            // Render the tile texture
            SDL_RenderTexture(renderer, tileset.texture, &srcRect, &destRect);
        } else {
            // Unresolved tile ID — render magenta fallback rectangle
            SDL_SetRenderDrawColor(renderer, m_fallbackR, m_fallbackG, m_fallbackB, config.alpha);
            SDL_RenderFillRect(renderer, &destRect);
        }
    }

    // Remove clip rect after rendering
    viewport.RemoveClip(renderer);
}

void TileRenderer::RenderBlockingOverlay(
    SDL_Renderer* renderer,
    const Tileset& tileset,
    const JigsawMap& map,
    const Viewport& viewport,
    const Camera& camera,
    const MapLayerConfig& config)
{
    if (!viewport.IsValid()) {
        return;
    }

    if (tileset.blockers.empty()) {
        return;
    }

    viewport.ApplyClip(renderer);

    const ViewportRect& vp = viewport.GetRect();
    const float camX = camera.GetX();
    const float camY = camera.GetY();
    const float pivotX = config.pivotX;
    const float pivotY = config.pivotY;
    const float offsetX = config.offsetX;
    const float offsetY = config.offsetY;
    const float scale = config.scale;

    float screenOriginX = static_cast<float>(vp.x) + pivotX * static_cast<float>(vp.width) + offsetX;
    float screenOriginY = static_cast<float>(vp.y) + pivotY * static_cast<float>(vp.height) + offsetY;

    // World-space coordinates at viewport edges
    float worldLeft = camX + (static_cast<float>(vp.x) - screenOriginX) / scale;
    float worldTop = camY + (static_cast<float>(vp.y) - screenOriginY) / scale;
    float worldRight = camX + (static_cast<float>(vp.x + vp.width) - screenOriginX) / scale;
    float worldBottom = camY + (static_cast<float>(vp.y + vp.height) - screenOriginY) / scale;

    float queryW = worldRight - worldLeft;
    float queryH = worldBottom - worldTop;

    std::vector<const PlacedTile*> visibleTiles = map.QueryRect(worldLeft, worldTop, queryW, queryH);

    // Set semi-transparent red for blocking overlay
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 200, 0, 0, 100);

    for (size_t i = 0; i < visibleTiles.size(); ++i) {
        const PlacedTile& tile = *visibleTiles[i];

        // Check if this tile is in the blockers set
        if (tileset.blockers.count(tile.tileId) == 0) {
            continue;
        }

        // Compute screen position from world position
        float destX = screenOriginX + (tile.x - camX) * scale;
        float destY = screenOriginY + (tile.y - camY) * scale;
        float destW = tile.w * scale;
        float destH = tile.h * scale;

        SDL_FRect destRect = { destX, destY, destW, destH };
        SDL_RenderFillRect(renderer, &destRect);
    }

    viewport.RemoveClip(renderer);
}
