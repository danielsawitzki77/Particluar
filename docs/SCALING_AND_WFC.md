# Scaling and WFC Adjacency — Known Limitations

## Three-Level Scaling

Tile rendering size is computed as:

```
final_size = base_tile_size * layer_scale * sheet_scale * tile_scale
```

Where:
- **layer_scale** — configured at runtime per `MapLayer` (default 1.0)
- **sheet_scale** — stored in sidecar JSON root as `"scale": 1.0`
- **tile_scale** — stored per tile in sidecar JSON as `"scale": 1.0`

All three compound multiplicatively.

## WFC and Per-Tile Scale

**Known limitation:** The WFC (Wave Function Collapse) generator currently ignores
per-tile scale during generation. All tiles are treated as occupying the same grid
cell size regardless of their individual `scale` values.

This means:
- Tiles with different per-tile scales will still be placed as if they were the same
  grid size during WFC generation.
- Visually, tiles with `scale != 1.0` may appear larger or smaller than their grid cell,
  potentially overlapping neighbors or leaving gaps.
- For best results with WFC, keep per-tile scale at 1.0 or use uniform scale values
  across all tiles in a tileset used for generation.

Tiles with different scales CAN still be valid neighbors if their scaled sizes match at
the boundary, but the WFC solver does not currently enforce or verify this constraint.

## Tile Overlap Rules

- **Within one layer:** Tiles occupy distinct grid cells. No overlapping allowed.
- **Between layers:** Layers are fully independent and CAN overlap freely since they
  each have their own grid, offset, scale, and z-depth.
