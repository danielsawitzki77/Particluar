# Design Document

## Overview

This design describes the architecture of the redesigned Level Editor UI. The editor is a single-page web application (HTML + vanilla JS) served by the existing Node.js/TypeScript server. All changes are client-side within `app.js` and `index.html`. No new server endpoints are needed.

## Architecture

### Component Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│ Level Editor Tab                                                 │
│ ┌────────────────────────┐  ┌──────────────────────────────────┐│
│ │ Sidebar (320px)        │  │ Map Canvas Area (flex)           ││
│ │                        │  │                                  ││
│ │ ┌──────────────────┐   │  │  ┌────────────────────────────┐ ││
│ │ │ Controls Panel   │   │  │  │ Canvas (zoomed)            │ ││
│ │ │ - Tileset select │   │  │  │ - Placed tiles             │ ││
│ │ │ - Grid params    │   │  │  │ - Grid overlay (free mode) │ ││
│ │ │ - Zoom           │   │  │  │ - Blocker overlay          │ ││
│ │ │ - Label filter   │   │  │  │ - Hover indicators         │ ││
│ │ │ - Blocker toggle │   │  │  └────────────────────────────┘ ││
│ │ └──────────────────┘   │  │                                  ││
│ │                        │  └──────────────────────────────────┘│
│ │ ┌──────────────────┐   │                                      │
│ │ │ Selection Slot   │   │                                      │
│ │ │ (pick from map)  │   │                                      │
│ │ └──────────────────┘   │                                      │
│ │                        │                                      │
│ │ ┌──────────────────┐   │                                      │
│ │ │ Tile Detail Panel│   │                                      │
│ │ │ (read-only info) │   │                                      │
│ │ └──────────────────┘   │                                      │
│ │                        │                                      │
│ │ ┌──────────────────┐   │                                      │
│ │ │ Tile Palette     │   │                                      │
│ │ │ (scrollable)     │   │                                      │
│ │ │ - thumbnails     │   │                                      │
│ │ │ - direction arrow│   │                                      │
│ │ └──────────────────┘   │                                      │
│ └────────────────────────┘                                      │
└─────────────────────────────────────────────────────────────────┘
```

### State Model

```javascript
// Editor state
let leMode = 'free';           // 'free' | 'picking' | 'constrained'
let leSlottedTile = null;      // { tile_id, x, y, w, h } — the placed tile picked from map
let leSelectedPaletteId = null;// tile ID selected in palette (for detail display or free placement)
let lePlacedTiles = [];        // Array of { tile_id, x, y, w, h }
let leZoom = 1.0;
let leGridCellW = 16, leGridCellH = 16;
let leGridOffX = 0, leGridOffY = 0;
let leShowBlockers = false;
let leHoveredPaletteId = null; // for hover warning indicator
```

### Mode Transitions

```
┌──────────┐  click empty slot   ┌──────────┐  click tile on map   ┌─────────────┐
│   FREE   │ ──────────────────> │ PICKING  │ ────────────────────> │ CONSTRAINED │
│          │ <────────────────── │          │                       │             │
└──────────┘  (click slot again  └──────────┘                       └─────────────┘
                or ESC)                                                    │
                     ^                                                     │
                     └──── click occupied slot ────────────────────────────┘
```

## Data Flow

### Adjacency Filtering Algorithm

```
function getCompatibleTiles(slottedTile, allTiles):
    slottedDef = findTileDef(slottedTile.tile_id)
    results = []
    
    for each direction in [up, down, left, right]:
        adjacencyList = slottedDef.adjacency[direction]
        if adjacencyList is empty:
            continue  // unconstrained = not shown in filtered mode
        
        oppositeDir = opposite(direction)
        
        for each neighborId in adjacencyList:
            neighborDef = findTileDef(neighborId)
            // Check reciprocity
            neighborOpposite = neighborDef.adjacency[oppositeDir]
            if neighborOpposite is empty OR neighborOpposite includes slottedDef.id:
                results.push({ tile: neighborDef, direction: direction })
                break  // first direction wins for this tile
    
    // Deduplicate: if a tile appears in multiple directions, keep first found
    return deduplicate(results, by: tile.id)
```

### Auto-Placement Position Calculation

```
function computePlacementPosition(slottedTile, candidateDef, direction):
    switch direction:
        case 'right':
            x = slottedTile.x + slottedTile.w
            y = slottedTile.y
        case 'left':
            x = slottedTile.x - candidateDef.source_rect.w * scale
            y = slottedTile.y
        case 'down':
            x = slottedTile.x
            y = slottedTile.y + slottedTile.h
        case 'up':
            x = slottedTile.x
            y = slottedTile.y - candidateDef.source_rect.h * scale
    return { x, y, w: candidateDef.source_rect.w * scale, h: candidateDef.source_rect.h * scale }
```

### Blocker Overlay Rendering

Blockers are defined in the tileset JSON as `blockers: [{ x, y, w, h }]` — these are in **tileset-local** pixel coordinates (relative to each tile's source_rect origin). For each placed tile, transform blockers to map-space:

```
for each placedTile in lePlacedTiles:
    tileDef = findTileDef(placedTile.tile_id)
    if tileDef has blockers:
        scaleX = placedTile.w / tileDef.source_rect.w
        scaleY = placedTile.h / tileDef.source_rect.h
        for each blocker in tileDef.blockers:
            mapX = placedTile.x + blocker.x * scaleX
            mapY = placedTile.y + blocker.y * scaleY
            mapW = blocker.w * scaleX
            mapH = blocker.h * scaleY
            drawRect(mapX, mapY, mapW, mapH, color='rgba(255,0,0,0.25)')
```

Note: Blockers are per-tileset (not per-tile). They are stored in the tileset JSON root as `"blockers": [{x,y,w,h}, ...]`. In the overlay, we render them at their absolute positions since they're defined in spritesheet-space. For level editor purposes, we'll match each blocker to placed tiles by checking if the blocker's source-space position falls within a tile's source_rect.

## File Changes

| File | Changes |
|------|---------|
| `tools/tileset-editor/public/index.html` | Widen sidebar, add selection slot, grid controls with step buttons, blocker toggle, remove Jigsaw/Constrain buttons |
| `tools/tileset-editor/public/app.js` | Rewrite level editor section: new state model, selection slot logic, filtered palette with arrows, auto-placement, blocker overlay, zoom improvements |

## Key Decisions

1. **No server changes** — all logic is client-side JS
2. **Single interaction flow** replaces three modes (grid, jigsaw, constrained)
3. **Direction priority** — up > down > left > right when a tile appears in multiple adjacency lists
4. **Blocker overlay** uses tileset-level blockers (from JSON root), rendered relative to placed tiles
5. **Grid only in free mode** — grid lines and grid snapping only apply when slot is empty
6. **Variable-size tiles** are the default — no mode switch needed
