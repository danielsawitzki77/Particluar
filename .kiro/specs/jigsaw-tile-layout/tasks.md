# Implementation Plan

## Overview

Implement the Jigsaw Tile Layout system bottom-up: start with the core data structures (PlacedTile, SpatialIndex, JigsawMap), then layer on serialization, rendering, WFC generation, chunk management, border overflow, and finally the level editor integration. Each task builds on the previous, enabling incremental testing via RapidCheck property-based tests.

## Tasks

- [ ] 1. PlacedTile struct and JigsawMap data structure with spatial index (Reqs 1, 4)
  - [ ] 1.1 Define `PlacedTile` struct with tile_id (string), x, y, w, h (float) in a new header `src/JigsawMap.h`
  - [ ] 1.2 Define `MapBoundary` struct with width_pixels and height_pixels
  - [ ] 1.3 Implement `JigsawMap` class with tile storage (vector), boundary management (Set/Clear/Has/Get), and tileset_id accessors
  - [ ] 1.4 Implement `JigsawMap::AddTile` with overlap detection (returns false if any existing tile overlaps)
  - [ ] 1.5 Implement `JigsawMap::RemoveTile` by position, updating the spatial index
  - [ ] 1.6 Implement `JigsawMap::QueryRect` and `QueryPoint` delegating to SpatialIndex
  - [ ] 1.7 Implement `JigsawMap::GetEdgeNeighbors` using adjacency detection (shared non-zero-length boundary segment)
  - [ ] 1.8 Write property-based tests (RapidCheck): Property 3 (no-overlap invariant), Property 5 (edge-to-edge placement)

- [ ] 2. SpatialIndex implementation — grid-based spatial hash (Req 1)
  - [ ] 2.1 Define `CellCoord` struct and `SpatialIndex` class in `src/SpatialIndex.h`
  - [ ] 2.2 Implement cell coordinate computation: `floor(pixel / cell_size)` with packed int64 key `(cx << 32) | (cy & 0xFFFFFFFF)`
  - [ ] 2.3 Implement `Insert` — compute all cells a tile overlaps and add tile index to each cell
  - [ ] 2.4 Implement `Remove` — remove tile index from all overlapping cells
  - [ ] 2.5 Implement `Query` — collect tile indices from all cells intersecting the query rect, deduplicate, and verify actual rect intersection
  - [ ] 2.6 Implement `Rebuild` — clear and re-insert all tiles
  - [ ] 2.7 Write property-based tests (RapidCheck): Property 1 (spatial query correctness — brute-force oracle comparison)

- [ ] 3. Effective size computation helpers (Req 2)
  - [ ] 3.1 Implement `ComputeEffectiveSize(source_w, source_h, tile_scale, sheet_scale, layer_scale)` returning (width, height) as floats
  - [ ] 3.2 Add helper functions `EffectiveWidth` and `EffectiveHeight` to WFCGenerator for use during generation (layer_scale = 1.0)
  - [ ] 3.3 Write property-based tests (RapidCheck): Property 2 (effective size equals source × tile_scale × sheet_scale × layer_scale)

- [ ] 4. JigsawMap JSON serialization/deserialization (Req 11)
  - [ ] 4.1 Implement `MapLoader::SerializeJigsawMap` — output JSON with format, tileset_id, optional boundary, and tiles array using picojson
  - [ ] 4.2 Implement `MapLoader::LoadJigsawMap` — parse JSON, validate each tile record, skip malformed records with SDL_Log, rebuild SpatialIndex
  - [ ] 4.3 Implement `MapLoader::SaveJigsawMap` — serialize and write to file
  - [ ] 4.4 Handle edge cases: NaN/Inf positions (skip as malformed), missing fields, wrong types
  - [ ] 4.5 Write property-based tests (RapidCheck): Property 8 (round-trip), Property 9 (malformed record graceful handling)

- [ ] 5. TileRenderer jigsaw layer rendering (Req 3)
  - [ ] 5.1 Add `TileRenderer::RenderJigsawLayer` method that queries SpatialIndex with viewport rect
  - [ ] 5.2 Render each returned tile at its absolute (x, y) position with (w, h) destination rect dimensions
  - [ ] 5.3 Apply layer alpha and sampling mode per MapLayerConfig
  - [ ] 5.4 Ensure deterministic render order (sort by y then x within a layer)
  - [ ] 5.5 Render fallback magenta rectangle when tile_id is not found in tileset
  - [ ] 5.6 Write unit tests: empty map renders nothing, single tile renders at correct position, viewport clipping

- [ ] 6. WFC jigsaw generator — gap-queue algorithm with backtracking (Reqs 4, 5, 6, 7, 12)
  - [ ] 6.1 Define `Gap` struct, `JigsawWFCParams`, `JigsawWFCResult`, and `WFCStatus` enum
  - [ ] 6.2 Implement gap priority queue with comparator (y first, then x — top-left-first ordering)
  - [ ] 6.3 Implement `GetCandidates` — filter tileset by effective size fit within gap, then by adjacency constraints with existing neighbors
  - [ ] 6.4 Implement `ValidateAdjacency` — check all shared boundary segments between candidate and placed tiles, validate adjacency rules in both directions, handle empty lists as unconstrained (Property 13)
  - [ ] 6.5 Implement `SplitGap` — after placing a tile, split remaining gap into sub-gaps (right remainder and bottom remainder)
  - [ ] 6.6 Implement backtracking via snapshot stack — on no-candidate condition, restore previous state and exclude last-tried tile
  - [ ] 6.7 Implement `GenerateJigsaw` main loop: seed RNG, initialize gap queue with target area, iterate until queue empty or contradiction
  - [ ] 6.8 Handle finite boundary: allow overflow at edges (tiles may extend beyond boundary), report Success when interior is covered
  - [ ] 6.9 Write property-based tests (RapidCheck): Property 4 (gap-free coverage), Property 6 (adjacency validation), Property 7 (deterministic generation)

- [ ] 7. ChunkManager for infinite maps (Req 8)
  - [ ] 7.1 Define `ChunkCoord` struct and `ChunkManager` class with configurable chunk size (default 512px)
  - [ ] 7.2 Implement chunk coordinate computation and packed key storage
  - [ ] 7.3 Implement `EnsureGenerated` — identify missing chunks covering a world-space rect, generate each via WFCGenerator with context from adjacent existing chunks
  - [ ] 7.4 Implement `QueryRect` — aggregate results from all chunks overlapping the query rect
  - [ ] 7.5 Implement cross-chunk seam handling: pass existing border tiles as context to WFCGenerator for new adjacent chunks
  - [ ] 7.6 Implement retry logic on contradiction (adjusted seed) and timeout cap per frame
  - [ ] 7.7 Write property-based tests (RapidCheck): Property 10 (chunk caching idempotence), Property 11 (cross-chunk seamless placement)

- [ ] 8. Border overflow rendering behavior (Req 9)
  - [ ] 8.1 Ensure TileRenderer does NOT clip tiles to the map boundary — tiles extending beyond boundary render fully
  - [ ] 8.2 Ensure Viewport clips rendering at its own boundaries (standard SDL viewport clipping)
  - [ ] 8.3 Verify placement algorithm permits tiles at boundary edges to overflow
  - [ ] 8.4 Write unit tests: tile at boundary edge extends beyond, viewport clips correctly

- [ ] 9. Level Editor — snap-to-edge placement and variable-size palette (Req 10)
  - [ ] 9.1 Implement `SnapEngine` class (TypeScript) with `findSnapPosition` method — finds nearest valid edge-aligned position within threshold
  - [ ] 9.2 Implement `wouldOverlap` check against existing placed tiles
  - [ ] 9.3 Implement `getValidPlacements` — enumerate all valid snap positions for a candidate tile
  - [ ] 9.4 Update tile palette to display thumbnails at actual proportional dimensions (not forced uniform size)
  - [ ] 9.5 Implement erase behavior: mark vacated space as unfilled gap
  - [ ] 9.6 Implement `MapSerializer` (TypeScript) for JSON I/O matching the C++ format
  - [ ] 9.7 Write property-based tests: Property 12 (snap engine validity — edge-aligned and no overlap)

- [ ] 10. Integration in PoC main.cpp — demonstrate jigsaw rendering and generation
  - [ ] 10.1 Add a demo mode in main.cpp that loads a tileset, runs WFC jigsaw generation on a small finite area, and renders the result
  - [ ] 10.2 Wire up camera/viewport to scroll across the generated jigsaw map
  - [ ] 10.3 Add keyboard toggle to switch between legacy grid rendering and jigsaw rendering for comparison
  - [ ] 10.4 Verify end-to-end: tileset load → WFC generate → serialize → deserialize → render matches original

## Task Dependency Graph

```json
{
  "waves": [
    [2],
    [1, 3],
    [4],
    [5, 6],
    [7, 8],
    [9],
    [10]
  ]
}
```

## Notes

- **Wave 1**: SpatialIndex is the foundational data structure — everything else depends on it.
- **Wave 2**: JigsawMap uses SpatialIndex; effective size helpers are independent but needed by WFC.
- **Wave 3**: Serialization depends on JigsawMap being complete.
- **Wave 4**: Renderer and WFC generator both depend on JigsawMap + serialization.
- **Wave 5**: ChunkManager wraps WFC; border overflow is a rendering refinement.
- **Wave 6**: Level editor depends on all core systems being stable.
- **Wave 7**: Integration demo ties everything together.
- All property-based tests use RapidCheck with minimum 100 iterations per property.
- Float comparison tolerance of 0.001f for adjacency detection (per design doc).
- C++14 compatible throughout; picojson for JSON; SDL3 for rendering.
