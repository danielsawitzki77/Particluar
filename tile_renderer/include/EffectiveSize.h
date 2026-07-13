#pragma once

#include <utility>

// Computes the effective rendered size of a tile given its source dimensions
// and the cumulative scale factors.
//
// effective_width  = src_w * tile_scale * sheetScale * layerScale
// effective_height = src_h * tile_scale * sheetScale * layerScale
//
// During WFC generation layerScale is always 1.0; it only applies at render time.
inline std::pair<float, float> ComputeEffectiveSize(
    float srcW,
    float srcH,
    float tileScale,
    float sheetScale,
    float layerScale)
{
    float combined = tileScale * sheetScale * layerScale;
    return std::make_pair(srcW * combined, srcH * combined);
}
