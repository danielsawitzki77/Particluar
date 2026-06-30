# Requirements Document

## Introduction

The Jigsaw Tile Layout system replaces the current uniform-grid tile placement in Particluar with a variable-size tile placement model. Tiles of differing pixel dimensions and individual scale values are placed edge-to-edge in a jigsaw-style layout with no interior gaps. This redesign affects the map data structure, the WFC generator, the tile renderer, and the level editor.

## Glossary

- **Tile_Renderer**: The C++ component responsible for drawing placed tiles to the screen via SDL3.
- **Jigsaw_Map**: The new map data structure storing placed tiles with absolute pixel positions and effective sizes, replacing the uniform 2D grid.
- **WFC_Generator**: The Wave Function Collapse algorithm component that procedurally fills space with tiles respecting adjacency and size constraints.
- **Level_Editor**: The web-based (Node.js/TypeScript) tool for manually placing and editing tiles on a map.
- **Placed_Tile**: A record containing a tile_id, absolute pixel position (x, y), and effective rendered size (width, height).
- **Effective_Size**: The final rendered pixel dimensions of a tile, computed as source_rect dimensions multiplied by tile_scale, sheet_scale, and layer_scale.
- **Spatial_Index**: A data structure enabling efficient lookup of tiles intersecting a given rectangular region.
- **Chunk**: A configurable rectangular region of the world used for on-demand generation and caching in infinite maps.
- **Adjacency_Rules**: The per-tile directional compatibility lists (up, down, left, right) defining which tiles may neighbor each other.
- **Viewport**: The rectangular visible area of the map rendered on screen.
- **Contradiction**: A WFC state where no valid tile can fill a remaining gap, requiring backtracking or failure.

## Requirements

### Requirement 1: Jigsaw Map Data Structure

**User Story:** As a developer, I want the map to store tiles with absolute pixel positions and effective sizes, so that tiles of varying dimensions can coexist without a uniform grid constraint.

#### Acceptance Criteria

1. THE Jigsaw_Map SHALL store each placed tile as a Placed_Tile record containing tile_id (string), x (float), y (float), width (float), and height (float).
2. THE Jigsaw_Map SHALL support efficient spatial queries that return all Placed_Tile records whose bounding rectangles intersect a given query rectangle.
3. WHEN a Placed_Tile is added to the Jigsaw_Map, THE Jigsaw_Map SHALL update the Spatial_Index to include the new tile.
4. WHEN a Placed_Tile is removed from the Jigsaw_Map, THE Jigsaw_Map SHALL update the Spatial_Index to exclude the removed tile.
5. THE Jigsaw_Map SHALL store an optional boundary definition (width_pixels, height_pixels) representing the finite map extent.
6. THE Jigsaw_Map SHALL store the tileset_id identifying the tileset used for the map.

### Requirement 2: Effective Size Computation

**User Story:** As a developer, I want each tile's rendered size to incorporate all three scale levels, so that tiles with different scale values produce correct jigsaw placements.

#### Acceptance Criteria

1. THE Tile_Renderer SHALL compute the Effective_Size of a tile as source_rect.w multiplied by tile_scale multiplied by sheet_scale multiplied by layer_scale for width, and source_rect.h multiplied by tile_scale multiplied by sheet_scale multiplied by layer_scale for height.
2. THE WFC_Generator SHALL use the Effective_Size (with layer_scale set to 1.0 during generation) when determining tile placement positions and gap dimensions.
3. WHEN tile_scale, sheet_scale, or layer_scale changes, THE Tile_Renderer SHALL recompute the Effective_Size for affected tiles.

### Requirement 3: Variable-Size Tile Rendering

**User Story:** As a developer, I want the renderer to draw each tile at its absolute pixel position with its individual effective size, so that variable-size tiles display correctly on screen.

#### Acceptance Criteria

1. THE Tile_Renderer SHALL draw each Placed_Tile at its stored (x, y) position using its stored (width, height) as the destination rectangle dimensions.
2. WHEN rendering a layer, THE Tile_Renderer SHALL query the Spatial_Index for tiles intersecting the current Viewport and render only those tiles.
3. THE Tile_Renderer SHALL apply the layer alpha value and sampling mode to each rendered tile.
4. THE Tile_Renderer SHALL render tiles in a deterministic order within a single layer.

### Requirement 4: Gap-Free Placement Constraint

**User Story:** As a developer, I want all placed tiles to share edges exactly with their neighbors, so that no visual gaps appear inside the map boundary.

#### Acceptance Criteria

1. WHEN a tile is placed to the right of an existing tile, THE placement algorithm SHALL set the new tile's x position to the existing tile's x plus the existing tile's width.
2. WHEN a tile is placed below an existing tile, THE placement algorithm SHALL set the new tile's y position to the existing tile's y plus the existing tile's height.
3. THE Jigsaw_Map SHALL enforce that no two Placed_Tiles overlap (their bounding rectangles have zero intersection area).
4. WHILE the map has a defined boundary, THE placement algorithm SHALL ensure all interior space within the boundary is covered by Placed_Tiles (no gaps inside the boundary).

### Requirement 5: Multi-Neighbor Adjacency

**User Story:** As a developer, I want a single tile's edge to be adjacent to multiple smaller tiles or one larger tile, so that the jigsaw layout supports truly variable dimensions.

#### Acceptance Criteria

1. THE WFC_Generator SHALL identify all tiles whose edges share a segment with a given tile's edge as adjacent neighbors for constraint propagation.
2. WHEN a tile's right edge spans the combined height of multiple smaller tiles stacked vertically, THE WFC_Generator SHALL validate Adjacency_Rules between the tile and each of those smaller neighbors individually.
3. WHEN a tile's bottom edge spans the combined width of multiple smaller tiles arranged horizontally, THE WFC_Generator SHALL validate Adjacency_Rules between the tile and each of those smaller neighbors individually.
4. THE WFC_Generator SHALL treat two tiles as edge-adjacent when they share a non-zero-length segment along their boundary.

### Requirement 6: Jigsaw-Aware WFC Generation

**User Story:** As a developer, I want the WFC algorithm to track available space and place tiles respecting their actual sizes, so that procedural generation produces gap-free jigsaw layouts.

#### Acceptance Criteria

1. THE WFC_Generator SHALL track unfilled rectangular regions as available space during generation.
2. WHEN selecting a tile for placement, THE WFC_Generator SHALL verify that the tile's Effective_Size fits within the available gap at the target position.
3. IF no tile from the tileset fits the remaining gap at a position, THEN THE WFC_Generator SHALL backtrack to the most recent placement and attempt an alternative tile.
4. IF backtracking exhausts all alternatives, THEN THE WFC_Generator SHALL report a Contradiction status.
5. WHEN a seed value greater than zero is provided, THE WFC_Generator SHALL produce identical output for identical inputs (deterministic generation).
6. WHEN a seed value of zero is provided, THE WFC_Generator SHALL use a non-deterministic random source.
7. THE WFC_Generator SHALL begin placement from the top-left corner of the target area and expand rightward and downward.

### Requirement 7: Finite Map Generation

**User Story:** As a developer, I want the WFC to fill a defined pixel area completely, so that finite maps have no interior gaps.

#### Acceptance Criteria

1. WHEN a finite boundary (width_pixels, height_pixels) is defined, THE WFC_Generator SHALL place tiles until all space within the boundary is covered.
2. WHILE generating a finite map, THE WFC_Generator SHALL allow tiles at the boundary edges to extend beyond the boundary (overflow).
3. THE WFC_Generator SHALL report Success status when all interior space within the boundary is filled with valid tiles.

### Requirement 8: Chunk-Based Infinite Generation

**User Story:** As a developer, I want infinite maps to generate in chunks on demand, so that the system scales to unbounded world sizes without pre-allocating memory.

#### Acceptance Criteria

1. WHEN no finite boundary is defined, THE Jigsaw_Map SHALL operate in infinite mode using chunk-based generation.
2. THE Jigsaw_Map SHALL divide the world into chunks of configurable pixel dimensions.
3. WHEN the Viewport moves into a region with no generated chunks, THE WFC_Generator SHALL generate new chunks to fill the visible area.
4. THE Jigsaw_Map SHALL cache previously generated chunks and return cached data for repeated queries.
5. THE WFC_Generator SHALL produce seamless tile placement across chunk boundaries (tiles may span multiple chunks).
6. WHEN generating a new chunk adjacent to existing chunks, THE WFC_Generator SHALL respect Adjacency_Rules with already-placed tiles at the chunk border.

### Requirement 9: Border Overflow Behavior

**User Story:** As a developer, I want tiles at map borders to extend past the boundary edge, so that the map edge appears natural rather than showing gaps.

#### Acceptance Criteria

1. WHILE a finite boundary is defined, THE Tile_Renderer SHALL render tiles that extend beyond the boundary without clipping them to the boundary.
2. THE Viewport SHALL clip rendering at its own boundaries regardless of tile overflow beyond the map boundary.
3. THE placement algorithm SHALL permit tiles at the boundary edge to have portions extending outside the defined map boundary.

### Requirement 10: Level Editor Variable-Size Tile Placement

**User Story:** As a level designer, I want the editor to support placing variable-size tiles that snap to valid positions, so that I can build jigsaw layouts manually.

#### Acceptance Criteria

1. WHEN the user selects a tile in the Level_Editor palette, THE Level_Editor SHALL display the tile thumbnail at its actual proportional dimensions (not forced to a uniform size).
2. WHEN the user moves a tile toward the map canvas, THE Level_Editor SHALL highlight valid placement positions where the tile's edges align with existing neighboring tiles.
3. WHEN the user places a tile, THE Level_Editor SHALL snap the tile to the nearest valid edge-to-edge position relative to existing neighbors.
4. WHEN the user erases a tile, THE Level_Editor SHALL mark the vacated space as an unfilled gap requiring a replacement tile.
5. THE Level_Editor SHALL prevent placement at positions that would cause overlap with existing Placed_Tiles.

### Requirement 11: Map Serialization

**User Story:** As a developer, I want the jigsaw map to serialize to and deserialize from JSON, so that maps can be saved and loaded reliably.

#### Acceptance Criteria

1. THE MapLoader SHALL serialize a Jigsaw_Map to a JSON object containing the tileset_id, optional boundary dimensions, and an array of Placed_Tile records.
2. THE MapLoader SHALL deserialize a JSON object into a Jigsaw_Map, reconstructing the Spatial_Index from the loaded Placed_Tile records.
3. FOR ALL valid Jigsaw_Map instances, serializing then deserializing SHALL produce an equivalent Jigsaw_Map (round-trip property).
4. IF the JSON contains an invalid or malformed Placed_Tile record, THEN THE MapLoader SHALL log a descriptive error and skip the invalid record.

### Requirement 12: Adjacency Rule Validation With Partial Edge Contact

**User Story:** As a developer, I want adjacency rules to be validated even when tiles share only a partial edge, so that the jigsaw layout enforces constraints correctly regardless of size differences.

#### Acceptance Criteria

1. WHEN two tiles share any non-zero-length segment along a common boundary, THE system SHALL treat them as adjacent for Adjacency_Rules validation.
2. THE WFC_Generator SHALL validate adjacency for every pair of tiles sharing a boundary segment, regardless of whether the shared segment covers the full edge of either tile.
3. IF a tile's adjacency list for a direction is empty, THEN all tiles are permitted as neighbors in that direction (unconstrained), and the reciprocal rule applies.
