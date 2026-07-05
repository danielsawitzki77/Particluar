# Requirements Document

## Introduction

Redesign the Level Editor UI in the Particluar web-based tileset editor (`tools/tileset-editor/`) to replace the current Jigsaw Mode toggle and Constrain Neighbors toggle with a unified "selection slot" interaction flow. The redesigned editor features a wider sidebar, tile detail display, grid parameter controls matching the configurator's UI, an intelligent adjacency-based filtered palette with auto-placement, and a blocker overlay display.

## Glossary

- **Level_Editor**: The Level Editor tab within the Particluar tileset editor web application
- **Sidebar**: The left-side panel of the Level Editor containing controls, the selection slot, and the tile palette
- **Selection_Slot**: A dedicated UI element above the tile palette that holds a reference tile picked from the map canvas
- **Tile_Palette**: The scrollable grid of tile thumbnails in the sidebar used for choosing tiles to place
- **Map_Canvas**: The main drawing area where tiles are placed to compose a level
- **Filtered_Palette**: The state of the Tile Palette when it shows only tiles compatible with the tile in the Selection Slot
- **Compatible_Tile**: A tile that satisfies reciprocal adjacency constraints with the Selection Slot tile in exactly one direction
- **Reciprocal_Adjacency**: The rule that both sides must allow each other — tile A lists tile B in direction D, AND tile B lists tile A in the opposite direction; empty adjacency lists mean unconstrained
- **Direction_Arrow**: A visual indicator on a palette tile thumbnail showing in which direction (↑↓←→) it is allowed relative to the slotted tile
- **Grid_Parameters**: The set of Cell Width, Cell Height, Offset X, and Offset Y values controlling the placement grid in free placement mode
- **Step_Buttons**: The ±1/±8/±16 increment/decrement buttons used for adjusting numeric spinner values
- **Pick_From_Map_Mode**: An interaction mode where the next click on the Map Canvas picks the clicked tile into the Selection Slot instead of placing a tile
- **Free_Placement_Mode**: The default placement mode (slot empty) where any tile can be placed on the grid
- **Constrained_Mode**: The mode active when the Selection Slot contains a tile — palette is filtered, auto-placement applies
- **Auto_Placement**: The behavior where selecting a compatible tile from the filtered palette automatically places it adjacent to the slotted tile on the map
- **Tile_Data**: The read-only metadata associated with a tile, including its id, source_rect, labels, and adjacency neighbors
- **Blocker_Overlay**: A semi-transparent visual layer rendered on top of the map canvas showing blocking rectangles from the tileset data

## Requirements

### Requirement 1: Wider Sidebar

**User Story:** As a level designer, I want a wider sidebar so that tile details and controls have enough horizontal space to be legible.

#### Acceptance Criteria

1. THE Level_Editor SHALL render the Sidebar with a minimum width of 320 pixels
2. THE Sidebar SHALL maintain its width when the browser window is resized, without overlapping the Map Canvas

### Requirement 2: Tile Data Display on Selection

**User Story:** As a level designer, I want to see all metadata of a selected palette tile so that I understand its properties without switching to the configurator tab.

#### Acceptance Criteria

1. WHEN a tile is selected in the Tile Palette, THE Level_Editor SHALL display the tile's id, source_rect dimensions, labels, and adjacency neighbor lists in a read-only detail panel within the Sidebar
2. WHEN no tile is selected in the Tile Palette, THE Level_Editor SHALL display a placeholder message in the detail panel indicating no tile is selected
3. THE Level_Editor SHALL display adjacency data grouped by direction (up, down, left, right), showing neighbor tile IDs for each direction
4. THE detail panel SHALL be read-only — editing tile metadata is only done in the Tileset Configurator tab

### Requirement 3: Grid Parameter Controls

**User Story:** As a level designer, I want grid size and offset spinners with ±1/±8/±16 step buttons so that I can quickly adjust the placement grid.

#### Acceptance Criteria

1. THE Level_Editor SHALL provide numeric spinner inputs for Cell Width, Cell Height, Offset X, and Offset Y within the Sidebar
2. THE Level_Editor SHALL provide Step Buttons with increments of ±1, ±8, and ±16 for each Grid Parameter spinner
3. THE Step Buttons SHALL match the visual style and layout used in the Tileset Configurator controls bar
4. WHEN a Grid Parameter value is changed, THE Level_Editor SHALL re-render the Map Canvas grid lines to reflect the updated parameters
5. Grid Parameters SHALL only affect Free Placement Mode (when the Selection Slot is empty)
6. Grid lines SHALL NOT be displayed while in Constrained Mode (when the Selection Slot contains a tile)

### Requirement 4: Selection Slot UI

**User Story:** As a level designer, I want a dedicated slot above the palette to hold a tile picked from the map so that I can use it as an anchor for adjacency-based placement.

#### Acceptance Criteria

1. THE Level_Editor SHALL display the Selection Slot as a visible rectangular area above the Tile Palette in the Sidebar
2. WHILE the Selection Slot is empty, THE Level_Editor SHALL display a visual indicator (dashed outline and placeholder text such as "Click to pick from map") communicating that a tile can be picked
3. WHILE the Selection Slot contains a tile, THE Level_Editor SHALL render a thumbnail of that tile's sprite within the slot, along with the tile's ID

### Requirement 5: Pick From Map Mode Activation

**User Story:** As a level designer, I want to click the selection slot to enter pick-from-map mode so that I can choose an already-placed tile as my adjacency reference.

#### Acceptance Criteria

1. WHEN the user clicks the Selection Slot while it is empty, THE Level_Editor SHALL enter Pick From Map Mode
2. WHEN the user clicks the Selection Slot while it contains a tile, THE Level_Editor SHALL clear the slot and return to Free Placement Mode
3. WHILE in Pick From Map Mode, THE Level_Editor SHALL display a visual cue (cursor change and slot highlight) indicating the mode is active
4. WHILE in Pick From Map Mode, WHEN the user clicks a tile on the Map Canvas, THE Level_Editor SHALL copy that tile's identity and position into the Selection Slot and exit Pick From Map Mode, entering Constrained Mode

### Requirement 6: Filtered Palette with Direction Arrows

**User Story:** As a level designer, I want the palette to filter down to compatible tiles and show direction arrows so that I can see which tiles fit and where they go relative to my selected tile.

#### Acceptance Criteria

1. WHILE the Selection Slot contains a tile, THE Tile Palette SHALL display only tiles that are listed in the slotted tile's non-empty adjacency direction lists AND reciprocally allow the slotted tile
2. IF a tile's adjacency list for a direction is empty (unconstrained), that direction SHALL NOT contribute compatible tiles to the filtered palette — unconstrained directions are handled via Free Placement Mode
3. WHILE the Selection Slot contains a tile, THE Tile Palette SHALL display a Direction Arrow (↑↓←→) on each compatible tile indicating the single allowed placement direction relative to the slotted tile
4. IF a compatible tile appears in multiple direction lists of the slotted tile, THE Level_Editor SHALL pick the first direction found (up→down→left→right priority) and display only that arrow
5. WHILE the Selection Slot is empty, THE Tile Palette SHALL display all available tiles without filtering or direction arrows
6. WHEN filtering results in zero compatible tiles, THE Tile Palette SHALL display a message indicating no compatible tiles were found

### Requirement 7: Auto-Placement of Compatible Tiles

**User Story:** As a level designer, I want compatible tiles to auto-place next to the slotted tile when selected from the filtered palette so that I can build adjacency-correct layouts efficiently.

#### Acceptance Criteria

1. WHILE in Constrained Mode, WHEN the user clicks a compatible tile in the filtered palette, THE Level_Editor SHALL automatically place that tile adjacent to the slotted tile on the Map Canvas in the direction indicated by the arrow
2. THE Level_Editor SHALL align the auto-placed tile edge-to-edge with the slotted tile based on the adjacency direction, accounting for differing tile dimensions
3. IF the target position for auto-placement is partially or fully occupied by an existing tile, THE Level_Editor SHALL remove the existing tile and place the new one
4. WHILE hovering a compatible tile in the filtered palette whose auto-placement would remove an existing tile, THE Level_Editor SHALL display a warning indicator on the Map Canvas highlighting the tile that would be removed
5. WHEN a tile is auto-placed, THE Level_Editor SHALL update the Map Canvas to render the newly placed tile immediately

### Requirement 8: Removal of Legacy Mode Toggles

**User Story:** As a level designer, I want the old Jigsaw Mode and Constrain Neighbors toggles removed so that the UI is not cluttered with deprecated controls.

#### Acceptance Criteria

1. THE Level_Editor SHALL NOT display a "Jigsaw Mode" toggle button
2. THE Level_Editor SHALL NOT display a "Constrain Neighbors" toggle button
3. THE Level_Editor SHALL support variable-size tile placement as the default behavior without requiring a mode toggle

### Requirement 9: Return to Free Placement Mode

**User Story:** As a level designer, I want to clear the selection slot to return to unconstrained placement so that I can freely place any tile when I do not need adjacency guidance.

#### Acceptance Criteria

1. WHEN the user clicks the Selection Slot while it contains a tile, THE Level_Editor SHALL clear the Selection Slot
2. WHEN the Selection Slot is cleared, THE Level_Editor SHALL restore the Tile Palette to its unfiltered state showing all tiles
3. WHILE the Selection Slot is empty, THE Level_Editor SHALL allow the user to place any tile from the palette at any position on the Map Canvas, snapped to Grid Parameters
4. WHILE in Free Placement Mode, THE Level_Editor SHALL display grid lines on the Map Canvas based on current Grid Parameters

### Requirement 10: Zoom Control

**User Story:** As a level designer, I want zoom controls that are convenient and accessible so that I can inspect tiles at different scales.

#### Acceptance Criteria

1. THE Level_Editor SHALL provide +/− buttons in the Sidebar for zooming the Map Canvas
2. THE Level_Editor SHALL support mouse wheel zooming on the Map Canvas
3. THE Level_Editor SHALL support a zoom range of 0.25x to 4x with 0.25x increments
4. THE Level_Editor SHALL display the current zoom level as a label between the +/− buttons
5. ALL tile placement, picking, and rendering SHALL account for the current zoom level

### Requirement 11: Blocker Overlay Display

**User Story:** As a level designer, I want to see blocking rectangles overlaid on the map so that I understand which areas are impassable.

#### Acceptance Criteria

1. THE Level_Editor SHALL provide a toggle to show/hide the Blocker Overlay on the Map Canvas
2. WHEN enabled, THE Level_Editor SHALL render all blocking rectangles from the currently loaded tileset's `blockers` data as semi-transparent red rectangles overlaid on top of placed tiles
3. THE Blocker Overlay SHALL render blockers relative to each placed tile's position and dimensions (blockers are defined in tile-local coordinates in the tileset JSON)
4. THE Blocker Overlay SHALL update when tiles are added or removed from the map
5. THE Blocker Overlay SHALL respect the current zoom level
