#include "Viewport.h"
#include <cmath>

void Viewport::SetRect(const ViewportRect& rect)
{
    m_rect = rect;
}

const ViewportRect& Viewport::GetRect() const
{
    return m_rect;
}

bool Viewport::IsValid() const
{
    return m_rect.width > 0 && m_rect.height > 0;
}

VisibleTileRange Viewport::ComputeVisibleTiles(
    float camera_x, float camera_y,
    float pivot_x, float pivot_y,
    int tile_width, int tile_height) const
{
    VisibleTileRange range = {0, 0, 0, 0};

    if (tile_width <= 0 || tile_height <= 0 || !IsValid()) {
        return range;
    }

    // The pixel at (camera_x, camera_y) appears at viewport pixel
    // (pivot_x * width, pivot_y * height).
    // Top-left world pixel visible:
    float top_left_x = camera_x - pivot_x * static_cast<float>(m_rect.width);
    float top_left_y = camera_y - pivot_y * static_cast<float>(m_rect.height);

    // Compute tile range
    range.col_start = static_cast<int>(std::floor(top_left_x / static_cast<float>(tile_width)));
    range.col_end = static_cast<int>(std::floor((top_left_x + static_cast<float>(m_rect.width)) / static_cast<float>(tile_width)));
    range.row_start = static_cast<int>(std::floor(top_left_y / static_cast<float>(tile_height)));
    range.row_end = static_cast<int>(std::floor((top_left_y + static_cast<float>(m_rect.height)) / static_cast<float>(tile_height)));

    return range;
}

void Viewport::ApplyClip(SDL_Renderer* renderer) const
{
    SDL_Rect clipRect;
    clipRect.x = m_rect.x;
    clipRect.y = m_rect.y;
    clipRect.w = m_rect.width;
    clipRect.h = m_rect.height;
    SDL_SetRenderClipRect(renderer, &clipRect);
}

void Viewport::RemoveClip(SDL_Renderer* renderer) const
{
    SDL_SetRenderClipRect(renderer, NULL);
}
