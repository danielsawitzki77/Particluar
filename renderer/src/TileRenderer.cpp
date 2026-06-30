#include "TileRenderer.h"
#include <algorithm>
#include <vector>

void TileRenderer::RenderLayer(
    SDL_Renderer* renderer,
    const Tileset& tileset,
    const MapData& mapData,
    const VisibleTileRange& range,
    const Viewport& viewport,
    const Camera& camera,
    int tile_width, int tile_height,
    int layer,
    Uint8 alpha)
{
    (void)layer; // reserved for future multi-layer support

    // Skip rendering when viewport is invalid
    if (!viewport.IsValid()) {
        return;
    }

    // Apply clip rect to constrain drawing within viewport
    viewport.ApplyClip(renderer);

    const ViewportRect& vp = viewport.GetRect();
    const float cam_x = camera.GetX();
    const float cam_y = camera.GetY();
    const float pivot_x = camera.GetPivotX();
    const float pivot_y = camera.GetPivotY();

    // Screen offset: where the camera world position maps to on screen
    const float screen_origin_x = static_cast<float>(vp.x) + pivot_x * static_cast<float>(vp.width);
    const float screen_origin_y = static_cast<float>(vp.y) + pivot_y * static_cast<float>(vp.height);

    const int grid_height = static_cast<int>(mapData.grid.size());

    // Iterate visible tile range in ascending row/col order (ascending layer order)
    for (int row = range.row_start; row <= range.row_end; ++row) {
        // Skip out-of-bounds rows
        if (row < 0 || row >= grid_height) {
            continue;
        }

        const int grid_width = static_cast<int>(mapData.grid[row].size());

        for (int col = range.col_start; col <= range.col_end; ++col) {
            // Skip out-of-bounds columns
            if (col < 0 || col >= grid_width) {
                continue;
            }

            const std::string& tileId = mapData.grid[row][col];

            // Compute destination rect
            float dest_x = screen_origin_x + (static_cast<float>(col) * static_cast<float>(tile_width) - cam_x);
            float dest_y = screen_origin_y + (static_cast<float>(row) * static_cast<float>(tile_height) - cam_y);

            // Look up tile ID in tileset
            auto it = tileset.id_index.find(tileId);
            if (it != tileset.id_index.end()) {
                // Resolved tile — render texture with source rect
                const TileDef& tileDef = tileset.tiles[it->second];
                const SourceRect& src = tileDef.source_rect;

                // Three-level scaling for single-layer overload:
                // base_tile * sheet_scale * tile_scale (layer_scale is implicitly 1.0)
                float finalScale = tileset.sheet_scale * tileDef.scale;
                float destW = static_cast<float>(tile_width) * finalScale;
                float destH = static_cast<float>(tile_height) * finalScale;

                SDL_FRect destRect = { dest_x, dest_y, destW, destH };

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
                    dest_x, dest_y,
                    static_cast<float>(tile_width),
                    static_cast<float>(tile_height)
                };
                SDL_SetRenderDrawColor(renderer, m_fallback_r, m_fallback_g, m_fallback_b, alpha);
                SDL_RenderFillRect(renderer, &destRect);
            }
        }
    }

    // Remove clip rect after rendering
    viewport.RemoveClip(renderer);
}

void TileRenderer::SetFallbackColor(Uint8 r, Uint8 g, Uint8 b)
{
    m_fallback_r = r;
    m_fallback_g = g;
    m_fallback_b = b;
}

void TileRenderer::RenderLayers(
    SDL_Renderer* renderer,
    const std::vector<MapLayer>& layers,
    const Viewport& viewport,
    const Camera& camera,
    int base_tile_width, int base_tile_height)
{
    if (!viewport.IsValid()) {
        return;
    }

    // Sort layers by z_depth (lower = drawn first / behind).
    // Build index vector to avoid modifying the input.
    std::vector<size_t> sortedIndices(layers.size());
    for (size_t i = 0; i < layers.size(); ++i) {
        sortedIndices[i] = i;
    }
    std::sort(sortedIndices.begin(), sortedIndices.end(),
        [&layers](size_t a, size_t b) {
            return layers[a].GetConfig().z_depth < layers[b].GetConfig().z_depth;
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
        // The grid cell size is base_tile * layer_scale (tiles fill their cell)
        const float layerScale = cfg.scale;
        const float cellW = static_cast<float>(base_tile_width) * layerScale;
        const float cellH = static_cast<float>(base_tile_height) * layerScale;

        // Use the layer's own pivot and offset to compute the screen origin
        const float cam_x = camera.GetX();
        const float cam_y = camera.GetY();
        const float pivot_x = cfg.pivot_x;
        const float pivot_y = cfg.pivot_y;
        const float offset_x = cfg.offset_x;
        const float offset_y = cfg.offset_y;

        const float screen_origin_x = static_cast<float>(vp.x) + pivot_x * static_cast<float>(vp.width) + offset_x;
        const float screen_origin_y = static_cast<float>(vp.y) + pivot_y * static_cast<float>(vp.height) + offset_y;

        // Compute visible tile range for this layer using its scaled cell size
        // (we need integer tile_width/height for ComputeVisibleTiles, so use ceiling)
        int effectiveTileW = (cellW > 0.0f) ? static_cast<int>(cellW + 0.5f) : base_tile_width;
        int effectiveTileH = (cellH > 0.0f) ? static_cast<int>(cellH + 0.5f) : base_tile_height;
        if (effectiveTileW < 1) effectiveTileW = 1;
        if (effectiveTileH < 1) effectiveTileH = 1;

        VisibleTileRange range = viewport.ComputeVisibleTiles(
            cam_x - offset_x, cam_y - offset_y,
            pivot_x, pivot_y,
            effectiveTileW, effectiveTileH);

        const int grid_height = static_cast<int>(mapData.grid.size());

        for (int row = range.row_start; row <= range.row_end; ++row) {
            if (row < 0 || row >= grid_height) {
                continue;
            }

            const int grid_width = static_cast<int>(mapData.grid[row].size());

            for (int col = range.col_start; col <= range.col_end; ++col) {
                if (col < 0 || col >= grid_width) {
                    continue;
                }

                const std::string& tileId = mapData.grid[row][col];

                // Compute destination rect (grid cell position)
                float dest_x = screen_origin_x + (static_cast<float>(col) * cellW - cam_x * layerScale);
                float dest_y = screen_origin_y + (static_cast<float>(row) * cellH - cam_y * layerScale);

                // Look up tile ID in tileset
                auto it = tileset->id_index.find(tileId);
                if (it != tileset->id_index.end()) {
                    const TileDef& tileDef = tileset->tiles[it->second];
                    const SourceRect& src = tileDef.source_rect;

                    // Three-level scaling:
                    // final_size = base_tile_size * layer_scale * sheet_scale * tile_scale
                    float finalScale = layerScale * tileset->sheet_scale * tileDef.scale;
                    float finalW = static_cast<float>(base_tile_width) * finalScale;
                    float finalH = static_cast<float>(base_tile_height) * finalScale;

                    SDL_FRect destRect = { dest_x, dest_y, finalW, finalH };

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
                    SDL_FRect destRect = { dest_x, dest_y, cellW, cellH };
                    SDL_SetRenderDrawColor(renderer, m_fallback_r, m_fallback_g, m_fallback_b, cfg.alpha);
                    SDL_RenderFillRect(renderer, &destRect);
                }
            }
        }
    }

    // Remove clip rect after rendering all layers
    viewport.RemoveClip(renderer);
}
