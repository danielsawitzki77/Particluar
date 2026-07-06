# Implementation Tasks

## Task 1: Restructure HTML — wider sidebar, new controls, remove legacy toggles
- [ ] Widen `.le-sidebar` to 320px min-width
- [ ] Add grid parameter controls (Cell W, Cell H, Off X, Off Y) with ±1/±8/±16 step buttons matching configurator style
- [ ] Add Selection Slot HTML element above the palette (dashed outline, placeholder text)
- [ ] Add Tile Detail Panel below the selection slot (read-only info area)
- [ ] Add Blocker Overlay toggle checkbox
- [ ] Remove "Jigsaw Mode: Off" button
- [ ] Remove "Constrain Neighbors: Off" button
- [ ] Keep zoom +/− controls and label filter

**Requirements:** 1, 3, 4, 8, 10, 11

## Task 2: Implement editor state model and mode transitions
- [ ] Define new state variables: `leMode`, `leSlottedTile`, `lePlacedTiles`, grid params, zoom, `leShowBlockers`
- [ ] Remove old state: `leJigsawMode`, `leJigsawTiles`, `useJigsawRendering`, `leConstrainNeighbors`, `leHoverRow/Col`, `leGrid`
- [ ] Implement mode transitions: free → picking → constrained → free
- [ ] Selection Slot click handler: empty → enter picking mode; occupied → clear and return to free
- [ ] ESC key: cancel picking mode, return to free
- [ ] Status bar updates for each mode

**Requirements:** 4, 5, 8, 9

## Task 3: Implement Pick From Map Mode
- [ ] When in picking mode, change canvas cursor to crosshair with visual cue
- [ ] On canvas click in picking mode, find the placed tile under cursor
- [ ] Copy found tile's identity and position into `leSlottedTile`
- [ ] Transition to constrained mode
- [ ] Update Selection Slot UI to show tile thumbnail and ID
- [ ] Highlight the slotted tile on the canvas with a distinct border

**Requirements:** 5

## Task 4: Implement filtered palette with direction arrows
- [ ] Write `getCompatibleTiles(slottedTile)` function implementing the adjacency filtering algorithm
- [ ] In constrained mode, re-render palette showing only compatible tiles
- [ ] Draw direction arrows (↑↓←→) as overlay text on each palette thumbnail
- [ ] Direction priority: up > down > left > right (first match wins)
- [ ] Handle empty result: show "No compatible tiles found" message
- [ ] In free mode, render full unfiltered palette (existing behavior)

**Requirements:** 6

## Task 5: Implement auto-placement logic
- [ ] Write `computePlacementPosition(slottedTile, candidateDef, direction)` function
- [ ] On palette click in constrained mode: compute target position, check for overlap
- [ ] If overlap: remove existing tile at target position, then place new tile
- [ ] Add placed tile to `lePlacedTiles` array
- [ ] Re-render canvas immediately after placement
- [ ] After auto-placement, update slotted tile to the newly placed tile (chain placement)

**Requirements:** 7

## Task 6: Implement hover warning for tile overwrite
- [ ] Track `leHoveredPaletteId` on mouseover of palette tiles in constrained mode
- [ ] Compute would-be placement position for hovered tile
- [ ] If an existing tile occupies that position, render a warning indicator (red highlight/X) on the canvas
- [ ] Clear warning indicator on mouseout

**Requirements:** 7 (AC 3, 4)

## Task 7: Implement tile detail panel
- [ ] When a palette tile is clicked (in any mode), display its data in the detail panel
- [ ] Show: tile ID, source_rect (x, y, w, h), labels list, adjacency lists (up/down/left/right with IDs)
- [ ] Read-only — no edit controls
- [ ] Clear panel when selection changes to nothing

**Requirements:** 2

## Task 8: Implement free placement mode with grid
- [ ] Render grid lines on canvas based on leGridCellW/H and leGridOffX/Y (only when slot is empty)
- [ ] On canvas click in free mode: snap to grid, place selected palette tile
- [ ] Right-click to remove tile at cursor position
- [ ] Grid lines hidden when in constrained mode
- [ ] Grid parameters update canvas immediately via step button handlers

**Requirements:** 3, 9

## Task 9: Implement zoom
- [ ] Keep existing +/− buttons and mouse wheel zoom
- [ ] Apply zoom as canvas scale transform for rendering
- [ ] Adjust all mouse coordinate calculations for zoom
- [ ] Display current zoom level label
- [ ] Ensure tile placement, picking, and overlay all respect zoom

**Requirements:** 10

## Task 10: Implement blocker overlay
- [ ] Add toggle state `leShowBlockers`
- [ ] When enabled, after rendering placed tiles, render blocker overlay
- [ ] For each placed tile: look up its TileDef, find blockers that intersect its source_rect
- [ ] Transform blocker coordinates from source-space to map-space
- [ ] Render as semi-transparent red rectangles
- [ ] Respect zoom level
- [ ] Update overlay when tiles are added/removed

**Requirements:** 11

## Task 11: Canvas rendering consolidation
- [ ] Single `renderLECanvas()` function that handles all modes
- [ ] Render placed tiles at their absolute positions (variable size)
- [ ] Render grid overlay (free mode only)
- [ ] Render slotted tile highlight (constrained mode)
- [ ] Render hover warning (constrained mode)
- [ ] Render blocker overlay (when enabled)
- [ ] Apply zoom transform to all rendering

**Requirements:** 1, 3, 7, 9, 10, 11

## Task 12: Export and cleanup
- [ ] Export function: serialize `lePlacedTiles` to JSON (jigsaw format — array of {tile_id, x, y, w, h})
- [ ] Remove all dead code from old jigsaw mode, grid mode, constrain mode
- [ ] Remove unused HTML elements and CSS
- [ ] Test that tileset loading still works correctly

**Requirements:** 8, 9
