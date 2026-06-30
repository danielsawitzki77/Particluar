#pragma once

#include <SDL3/SDL.h>

struct ViewportRect {
    int x;       // screen-space left edge (pixels)
    int y;       // screen-space top edge (pixels)
    int width;   // viewport width (pixels)
    int height;  // viewport height (pixels)
};

struct VisibleTileRange {
    int col_start;  // first visible column (inclusive)
    int col_end;    // last visible column (inclusive)
    int row_start;  // first visible row (inclusive)
    int row_end;    // last visible row (inclusive)
};

class Viewport {
public:
    void SetRect(const ViewportRect& rect);
    const ViewportRect& GetRect() const;

    // Returns true if viewport has positive dimensions (renderable).
    bool IsValid() const;

    // Computes which tiles are potentially visible given camera and tile size.
    VisibleTileRange ComputeVisibleTiles(
        float camera_x, float camera_y,
        float pivot_x, float pivot_y,
        int tile_width, int tile_height) const;

    // Sets SDL clip rect for rendering. Call before drawing tiles.
    void ApplyClip(SDL_Renderer* renderer) const;

    // Removes SDL clip rect. Call after drawing tiles.
    void RemoveClip(SDL_Renderer* renderer) const;

private:
    ViewportRect m_rect = {0, 0, 0, 0};
};
