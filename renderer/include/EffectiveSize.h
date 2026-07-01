#pragma once

#include <utility>

// Computes the effective rendered size of a tile given its source dimensions
// and the cumulative scale factors.
//
// effective_width  = src_w * tile_scale * sheet_scale * layer_scale
// effective_height = src_h * tile_scale * sheet_scale * layer_scale
//
// During WFC generation layer_scale is always 1.0; it only applies at render time.
inline std::pair<float, float> ComputeEffectiveSize(
    float src_w,
    float src_h,
    float tile_scale,
    float sheet_scale,
    float layer_scale)
{
    float combined = tile_scale * sheet_scale * layer_scale;
    return std::make_pair(src_w * combined, src_h * combined);
}
