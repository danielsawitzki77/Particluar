#include "TileRenderer.h"

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
            SDL_FRect destRect = {
                dest_x,
                dest_y,
                static_cast<float>(tile_width),
                static_cast<float>(tile_height)
            };

            // Look up tile ID in tileset
            auto it = tileset.id_index.find(tileId);
            if (it != tileset.id_index.end()) {
                // Resolved tile — render texture with source rect
                const TileDef& tileDef = tileset.tiles[it->second];
                const SourceRect& src = tileDef.source_rect;

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
