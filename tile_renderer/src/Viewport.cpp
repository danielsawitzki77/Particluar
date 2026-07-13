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
    float cameraX, float cameraY,
    float pivotX, float pivotY,
    int tileWidth, int tileHeight) const
{
    VisibleTileRange range = {0, 0, 0, 0};

    if (tileWidth <= 0 || tileHeight <= 0 || !IsValid()) {
        return range;
    }

    // The pixel at (cameraX, cameraY) appears at viewport pixel
    // (pivotX * width, pivotY * height).
    // Top-left world pixel visible:
    float topLeftX = cameraX - pivotX * static_cast<float>(m_rect.width);
    float topLeftY = cameraY - pivotY * static_cast<float>(m_rect.height);

    // Compute tile range
    range.colStart = static_cast<int>(std::floor(topLeftX / static_cast<float>(tileWidth)));
    range.colEnd = static_cast<int>(std::floor((topLeftX + static_cast<float>(m_rect.width)) / static_cast<float>(tileWidth)));
    range.rowStart = static_cast<int>(std::floor(topLeftY / static_cast<float>(tileHeight)));
    range.rowEnd = static_cast<int>(std::floor((topLeftY + static_cast<float>(m_rect.height)) / static_cast<float>(tileHeight)));

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
