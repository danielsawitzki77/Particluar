# Requirements Document

## Introduction

A comprehensive, reusable 2D Map System module for the Particluar project. The system provides tile-based map rendering, tileset data management, procedural map generation via Wave Function Collapse, and companion tooling (CLI naming tool, web-based editor). The Renderer is a standalone static library within the existing Particluar.sln with zero dependencies on Particluar core logic, enabling reuse across future projects. Only basic SDL3 rendering and input hooks are required.

## Glossary

- **Renderer**: The static library (`.lib`) within Particluar.sln responsible for 2D tile map rendering, viewport management, and camera control. Has no dependency on Particluar application logic.
- **Tileset**: A collection of tile graphics stored as a single PNG spritesheet plus a JSON sidecar file that defines per-tile metadata.
- **Tile**: A single rectangular graphic within a Tileset spritesheet, identified by a unique string name/ID.
- **Sidecar_JSON**: The JSON metadata file accompanying a tileset PNG, containing tile source rectangles, names, and adjacency rules.
- **Adjacency_Rules**: Per-tile constraints defining which Tiles are valid neighbors in each cardinal direction (up, down, left, right).
- **Viewport**: A defined rectangular screen region where the Renderer draws map content, supporting alpha transparency and Z-depth layering.
- **Camera**: The logical position within world-space that determines which portion of the map is visible in the Viewport.
- **Center_Pivot**: The configurable anchor point on the Viewport that the Camera position maps to (e.g., center, top-left offset).
- **WFC_Generator**: The Wave Function Collapse (or constraint-based) map generation algorithm implemented in pure C++.
- **Map_File**: A JSON file representing a generated or hand-painted tile map, specifying tile IDs at grid positions.
- **Tile_Naming_Tool**: A CLI executable that parses a tileset PNG and Sidecar_JSON, auto-assigns unique logical names to unnamed tiles, and updates the JSON in place.
- **Web_Editor**: An HTML5/TypeScript/Node.js application serving two roles: Tileset Configurator and Level Editor.
- **Global_Config**: A JSON configuration file specifying system-wide defaults (tile sizes, viewport defaults, etc.) read by the Renderer at initialization.
- **Source_Rect**: The pixel coordinates (x, y, w, h) within a tileset PNG that define a single Tile's graphic region.

## Requirements

### Requirement 1: Renderer Static Library Integration

**User Story:** As a developer, I want the Renderer to be a static library within the existing Particluar.sln, so that the map system builds as part of the solution without requiring a separate repository or build system.

#### Acceptance Criteria

1. THE Renderer SHALL compile as a static library (.lib) project within Particluar.sln targeting x64 Debug and Release configurations, using the C++14 language standard.
2. THE Renderer SHALL have zero compile-time dependencies on Particluar application source files (src/ and include/App.h).
3. THE Renderer SHALL depend only on SDL3 headers for rendering and input functionality, and on picojson for JSON parsing, with no dependencies on SDL_image or stb.
4. WHEN the Particluar application project references the Renderer library, THE linker SHALL resolve all Renderer symbols without additional library dependencies beyond SDL3, picojson, and Windows platform libraries already required by SDL3 static linking.
5. THE Renderer project SHALL reside in a dedicated subdirectory within the Particluar repository and SHALL NOT require files from outside the repository to compile.

### Requirement 2: Global Configuration

**User Story:** As a developer, I want a global JSON configuration file for the Renderer, so that default tile sizes and viewport parameters are data-driven and adjustable without recompilation.

#### Acceptance Criteria

1. WHEN the Renderer initializes, THE Renderer SHALL parse the Global_Config JSON file as a root-level JSON object containing the required keys: "tile_width", "tile_height", "viewport_x", "viewport_y", "viewport_width", and "viewport_height", and apply the parsed values as default settings.
2. THE Global_Config SHALL define default tile width and height as integer values in the range 1 to 512 pixels inclusive.
3. THE Global_Config SHALL define default Viewport width and height as integer values in the range 1 to 7680 and 1 to 4320 pixels inclusive respectively, and Viewport position as integer x and y pixel coordinates (origin at top-left of the display) in the range 0 to 7680 and 0 to 4320 respectively.
4. IF the Global_Config file is missing, fails JSON parsing, is missing any required key, or contains a value outside its specified range, THEN THE Renderer SHALL ignore the entire file, fall back to hard-coded default values for all settings, and log a warning message via SDL_Log indicating the reason for the fallback.
5. THE Renderer SHALL satisfy the round-trip property: for any Global_Config object that passes validation, serializing it back to JSON and re-parsing the result SHALL produce a configuration object whose field values are identical to the original.

### Requirement 3: Viewport Rendering

**User Story:** As a developer, I want the Renderer to draw tiles within a constrained screen region with transparency and depth support, so that the map can coexist with other UI elements in the application window.

#### Acceptance Criteria

1. THE Renderer SHALL render tile content exclusively within a defined rectangular Viewport region specified by screen-space coordinates (x, y, width, height in pixels).
2. THE Renderer SHALL support alpha transparency for tiles rendered within the Viewport, where alpha values range from 0 (fully transparent) to 255 (fully opaque).
3. THE Renderer SHALL support Z-depth layering, drawing tiles in ascending layer index order so that tiles with a lower layer index are drawn first (behind) and tiles with a higher layer index are drawn last (in front).
4. WHEN a tile's bounding rectangle does not intersect the Viewport's visible area, THE Renderer SHALL skip rendering that tile.
5. WHILE the Viewport dimensions are set, THE Renderer SHALL clip all tile rendering to the Viewport boundary so that tiles partially overlapping the Viewport edge are drawn only within the Viewport region.
6. IF the Viewport width or height is set to zero or a negative value, THEN THE Renderer SHALL render no tiles and produce no visual output for that frame.

### Requirement 4: Camera System

**User Story:** As a developer, I want a camera with a customizable center pivot that determines which part of the world map is shown in the viewport, so that different games can anchor the view to different screen positions.

#### Acceptance Criteria

1. THE Camera SHALL maintain a world-space position (floating-point x, y) that determines the visible map region.
2. THE Renderer SHALL support a configurable Center_Pivot expressed as a normalized (x, y) pair where each component ranges from 0.0 to 1.0 inclusive, defaulting to (0.5, 0.5), that defines which point on the Viewport corresponds to the Camera position.
3. WHEN the Center_Pivot is set to (0.5, 0.5), THE Camera position SHALL correspond to the center of the Viewport.
4. WHEN the Camera position changes, THE Renderer SHALL recompute the visible tile region on the next frame such that the tile at the Camera world position is drawn at the screen pixel located at (Center_Pivot.x * Viewport_Width, Center_Pivot.y * Viewport_Height) measured from the top-left corner of the Viewport.
5. IF the Center_Pivot is set to a value where either component is less than 0.0 or greater than 1.0, THEN THE Renderer SHALL clamp each component to the range 0.0–1.0 before applying the pivot.

### Requirement 5: WASD Camera Scrolling (Proof of Concept)

**User Story:** As a developer, I want WASD keyboard input to scroll the camera inside the Particluar application, so that the map rendering and camera system can be validated interactively.

#### Acceptance Criteria

1. WHILE the Particluar application is running, THE application SHALL accept keyboard input events for W, A, S, and D keys via SDL3 keyboard state polling.
2. WHILE the W key is held, THE Camera SHALL move upward in world-space at a rate of scroll_speed pixels per second, scaled by the elapsed frame delta time.
3. WHILE the S key is held, THE Camera SHALL move downward in world-space at a rate of scroll_speed pixels per second, scaled by the elapsed frame delta time.
4. WHILE the A key is held, THE Camera SHALL move left in world-space at a rate of scroll_speed pixels per second, scaled by the elapsed frame delta time.
5. WHILE the D key is held, THE Camera SHALL move right in world-space at a rate of scroll_speed pixels per second, scaled by the elapsed frame delta time.
6. WHEN multiple WASD keys are held simultaneously, THE Camera SHALL apply each active direction's movement independently, resulting in combined (diagonal) movement.
7. THE scroll_speed SHALL be configurable via the Global_Config JSON file as a numeric value in pixels per second, with a hard-coded default of 200.0 pixels per second used when the key is absent or the value is not a positive finite number.

### Requirement 6: Tileset Data Schema

**User Story:** As a content creator, I want tilesets stored as PNG spritesheets with JSON sidecar files in a structured folder hierarchy, so that tile assets are self-describing and tooling can operate on them uniformly.

#### Acceptance Criteria

1. THE Tileset assets SHALL reside in the `assets\tilesets\` directory with one subfolder per tileset, where each subfolder name is a lowercase string of 1 to 64 characters using only the characters a–z, 0–9, and underscore.
2. THE Tileset subfolder SHALL contain exactly one PNG spritesheet file and one Sidecar_JSON file whose filename matches the PNG file's base name with a `.json` extension (e.g., `forest.png` paired with `forest.json`).
3. THE Sidecar_JSON SHALL define for each Tile: a Source_Rect (x, y, w, h in pixels where w >= 1 and h >= 1), a unique string name/ID of 1 to 64 characters (unique within the same Sidecar_JSON), and Adjacency_Rules specifying valid neighbor tile IDs for up, down, left, and right directions.
4. WHEN a Sidecar_JSON is parsed, THE Renderer SHALL validate that every Source_Rect's region (x to x+w and y to y+h) falls within the PNG image pixel dimensions.
5. IF a Source_Rect in a parsed Sidecar_JSON extends beyond the PNG image dimensions, THEN THE Renderer SHALL log a warning identifying the offending tile name/ID and skip that tile during loading.
6. IF a Sidecar_JSON contains a Tile with an empty or missing name/ID field, THEN THE Renderer SHALL log a warning and skip that tile during loading.
7. THE Tileset loader SHALL guarantee the round-trip property: for any valid Sidecar_JSON, parsing it into memory and serializing it back to JSON and parsing again SHALL produce a tileset definition with identical tile IDs, Source_Rects (same x, y, w, h values), and Adjacency_Rules (same neighbor IDs per direction) as the first parse.

### Requirement 7: Tileset Loading

**User Story:** As a developer, I want the Renderer to load tileset data from disk into memory, so that tiles are available for map rendering and generation.

#### Acceptance Criteria

1. WHEN a tileset path is provided, THE Renderer SHALL load the PNG spritesheet located in that subfolder into an SDL_Texture using IMG_LoadTexture, and SHALL store the resulting texture dimensions (width and height in pixels) for later sprite-rect calculation.
2. WHEN a tileset path is provided, THE Renderer SHALL parse the Sidecar_JSON file co-located in the same subfolder and construct an in-memory Tile definition for each entry, containing at minimum: id (string), Source_Rect (x, y, w, h), and Adjacency_Rules (up, down, left, right neighbor arrays).
3. IF the PNG file fails to load (IMG_LoadTexture returns NULL), THEN THE Renderer SHALL return false to the caller and log the file path and SDL error string via SDL_Log.
4. IF the Sidecar_JSON fails to parse (picojson returns a non-empty error string) or the root value is not the expected type, THEN THE Renderer SHALL return false to the caller and log the file path and parse error via SDL_Log.
5. IF an individual tile entry in the Sidecar_JSON is missing a required field or contains an invalid type, THEN THE Renderer SHALL skip that entry, log a warning identifying the entry index via SDL_Log, and continue processing remaining entries.

### Requirement 8: WFC Map Generator Core Algorithm

**User Story:** As a developer, I want a Wave Function Collapse map generator in pure C++, so that maps can be procedurally generated respecting tileset adjacency constraints.

#### Acceptance Criteria

1. THE WFC_Generator SHALL accept a tileset definition (tile set with Adjacency_Rules) and output grid dimensions (width and height, each between 1 and 1024 inclusive) as input.
2. THE WFC_Generator SHALL produce a fully-collapsed grid where every cell contains exactly one Tile ID.
3. THE WFC_Generator SHALL respect all Adjacency_Rules such that no horizontally or vertically adjacent cell pair (North, South, East, West) violates the defined neighbor constraints for the shared edge.
4. WHEN a contradiction occurs (a cell has zero valid options), THE WFC_Generator SHALL report failure and return an error status without modifying the output grid.
5. THE WFC_Generator SHALL accept an optional random seed parameter for deterministic generation.
6. WHEN the same seed, tileset, and grid dimensions are provided, THE WFC_Generator SHALL produce identical output.
7. THE WFC_Generator SHALL have zero dependencies on SDL3, operating purely on data structures and standard C++ library.
8. IF the provided tileset contains zero tiles, or grid dimensions are outside the range 1 to 1024, THEN THE WFC_Generator SHALL return an error status without attempting generation.
9. THE WFC_Generator SHALL select the next cell to collapse using the minimum entropy heuristic (the cell with the fewest remaining valid tile options).

### Requirement 9: Real-Time Map Generation Integration

**User Story:** As a developer, I want to invoke the WFC generator at runtime inside the Particluar application, so that maps can be generated dynamically during gameplay.

#### Acceptance Criteria

1. WHEN the Particluar application requests map generation, THE WFC_Generator SHALL execute and return a data structure containing grid width, grid height, tileset identifier, and a 2D array of Tile IDs identical in schema to Map_File.
2. THE WFC_Generator SHALL complete generation for a 64x64 grid within 5 seconds on an x64 desktop CPU running at 2.0 GHz or above with at least 4 GB of available RAM.
3. WHEN generation completes successfully, THE Renderer SHALL display the resulting map on the next rendered frame without requiring application restart.
4. IF the WFC_Generator encounters a contradiction during runtime generation, THEN THE Particluar application SHALL receive an error status and THE Renderer SHALL continue displaying the previously active map unchanged.

### Requirement 10: Offline CLI Map Generator Tool

**User Story:** As a developer, I want a CLI tool that pre-generates map files to disk using the WFC algorithm, so that maps can be baked offline for distribution or testing.

#### Acceptance Criteria

1. THE CLI tool SHALL accept command-line arguments for: tileset path, output file path, grid width (integer, 1 to 1024), grid height (integer, 1 to 1024), and an optional integer seed.
2. WHEN invoked with valid arguments and no seed specified, THE CLI tool SHALL generate a non-deterministic seed and write a Map_File JSON to the specified output path.
3. WHEN invoked with valid arguments and a seed specified, THE CLI tool SHALL use that seed for deterministic generation and write a Map_File JSON to the specified output path.
4. IF the tileset path does not exist or its Sidecar_JSON fails to parse, THEN THE CLI tool SHALL print an error message indicating the cause to stderr and exit with a non-zero code.
5. IF grid width or grid height is outside the range 1 to 1024, THEN THE CLI tool SHALL print an error message to stderr and exit with a non-zero code.
6. IF the WFC_Generator encounters a contradiction during generation, THEN THE CLI tool SHALL print an error message to stderr indicating generation failure and exit with a non-zero code without writing an output file.
7. THE CLI tool SHALL share the same WFC_Generator source code as the runtime integration (no code duplication).
8. THE CLI tool SHALL build as a separate executable project within Particluar.sln.

### Requirement 11: Map File Format

**User Story:** As a developer, I want a well-defined JSON format for map files, so that generated and hand-painted maps are interchangeable between tools.

#### Acceptance Criteria

1. THE Map_File JSON SHALL contain: grid width (integer, 1 to 4096), grid height (integer, 1 to 4096), tileset identifier (string, 1 to 255 characters), and a 2D array of Tile IDs stored in row-major order.
2. THE Map_File JSON SHALL store tile IDs as strings matching the names defined in the Sidecar_JSON, where each Tile ID is between 1 and 128 characters in length.
3. WHEN the Renderer loads a Map_File, THE Renderer SHALL resolve each Tile ID against the referenced tileset.
4. IF a Map_File references a Tile ID not found in the tileset, THEN THE Renderer SHALL log a warning identifying the unresolved Tile ID and cell position, and render a visually distinct fallback tile for that cell.
5. THE Map_File JSON SHALL satisfy the round-trip property: parsing a valid Map_File, serializing the result back to JSON, and parsing again SHALL produce a map with identical grid width, grid height, tileset identifier, and the same Tile ID string at every grid position.
6. IF the tileset identifier in a Map_File does not resolve to an existing tileset, THEN THE Renderer SHALL report an error indicating the missing tileset and SHALL NOT render the map.
7. IF the 2D array row count does not equal the declared grid height, or any row length does not equal the declared grid width, THEN the consuming tool SHALL reject the Map_File and report an error indicating the dimension mismatch.
8. IF the Map_File contains malformed JSON that cannot be parsed, THEN the consuming tool SHALL reject the file and report an error indicating the parse failure location.

### Requirement 12: Tile Naming CLI Tool

**User Story:** As a content creator, I want a CLI tool that auto-assigns unique logical names to unnamed tiles in a tileset JSON, so that every tile has an identifier for use in adjacency rules and map files.

#### Acceptance Criteria

1. WHEN invoked with a tileset folder path, THE Tile_Naming_Tool SHALL parse the Sidecar_JSON from that folder.
2. THE Tile_Naming_Tool SHALL identify all Tile entries that have an empty or missing name/ID field.
3. THE Tile_Naming_Tool SHALL assign a unique string name to each unnamed tile using a deterministic naming scheme (e.g., `tile_<row>_<col>` based on Source_Rect position in the spritesheet), ensuring generated names do not collide with any existing tile names in the same Sidecar_JSON.
4. WHEN naming is complete, THE Tile_Naming_Tool SHALL write the updated Sidecar_JSON back to disk in place, preserving all existing data for already-named tiles.
5. THE Tile_Naming_Tool SHALL build as a separate executable project within Particluar.sln.
6. IF the Sidecar_JSON does not exist at the specified path or contains malformed JSON, THEN THE Tile_Naming_Tool SHALL print an error to stderr and exit with a non-zero code.
7. IF all tiles in the Sidecar_JSON already have valid names, THEN THE Tile_Naming_Tool SHALL exit with code 0 without modifying the file.

### Requirement 13: Web Editor — Tileset Configurator

**User Story:** As a content creator, I want a browser-based tool to visually browse tileset folders, adjust grid/offset parameters, and define neighbor rules per tile, so that tileset authoring is interactive rather than manual JSON editing.

#### Acceptance Criteria

1. THE Web_Editor Tileset Configurator SHALL display the contents of the `assets\tilesets\` directory, listing available tileset subfolders.
2. WHEN a tileset is selected, THE Tileset Configurator SHALL render the PNG spritesheet with a grid overlay whose cell dimensions and offset match the current grid parameter values.
3. THE Tileset Configurator SHALL allow the user to adjust tile grid cell width (1–2048 pixels), cell height (1–2048 pixels), and pixel offset x and y (0 to spritesheet dimension minus 1), and SHALL update the grid overlay within 200 milliseconds of any parameter change.
4. WHEN the user selects a tile cell, THE Tileset Configurator SHALL display that tile's current Adjacency_Rules for north, south, east, and west directions, and allow editing the list of valid neighbor tile identifiers for each direction.
5. WHEN the user saves changes, THE Tileset Configurator SHALL write the updated Sidecar_JSON to the tileset's subfolder on disk.
6. IF the Sidecar_JSON write fails, THEN THE Tileset Configurator SHALL display an error message indicating the failure reason and SHALL preserve the user's unsaved edits in the editor state.
7. IF the `assets\tilesets\` directory is empty or contains no valid tileset subfolders, THEN THE Tileset Configurator SHALL display a message indicating that no tilesets are available.
8. THE Web_Editor SHALL be implemented using HTML5, TypeScript, and Node.js.

### Requirement 14: Web Editor — Level Editor

**User Story:** As a level designer, I want a browser-based grid painter to place and erase tiles using loaded tileset data, so that maps can be hand-crafted visually and exported as Map_File JSON.

#### Acceptance Criteria

1. WHEN the user sets grid dimensions, THE Level Editor SHALL accept width and height values between 1 and 256 tiles inclusive, and SHALL display the editable grid canvas at the specified dimensions before painting begins.
2. WHEN a tileset is loaded, THE Level Editor SHALL present a tile palette showing all available tiles from the Sidecar_JSON.
3. WHEN the user clicks a grid cell with a tile selected from the palette, THE Level Editor SHALL place that tile at the clicked position.
4. WHEN the user right-clicks a grid cell, THE Level Editor SHALL erase the tile at that position (set to empty).
5. WHEN the user triggers export, THE Level Editor SHALL generate a Map_File JSON containing the grid width, grid height, and a tile layer that lists the tile ID for each occupied cell and a null or empty marker for each unoccupied cell, and SHALL offer the file for download.
6. THE Level Editor SHALL validate that the exported Map_File references only tile IDs present in the loaded tileset.
7. IF the user clicks a grid cell when no tile is selected in the palette or no tileset is loaded, THEN THE Level Editor SHALL ignore the click and leave the cell unchanged.

### Requirement 15: Optional PowerShell/Windows Forms Wrapper for CLI

**User Story:** As a developer on Windows, I want an optional PowerShell or Windows Forms dialog wrapping the offline CLI generator, so that map generation can be triggered through a simple GUI without memorizing command-line arguments.

#### Acceptance Criteria

1. WHERE the PowerShell wrapper is available, THE wrapper SHALL present a text input field for tileset path, a text input field for output path, a numeric input field for grid width accepting positive integers from 1 to 1024, a numeric input field for grid height accepting positive integers from 1 to 1024, and a text input field for seed.
2. WHERE the PowerShell wrapper is available, WHEN the user clicks Generate and all five input fields contain non-empty values and grid width and grid height are positive integers within the accepted range, THE wrapper SHALL invoke the offline CLI tool with the specified arguments.
3. WHERE the PowerShell wrapper is available, IF the user clicks Generate and any required field is empty or grid width or grid height is not a positive integer within the accepted range, THEN THE wrapper SHALL display an error message indicating which fields are invalid without invoking the CLI tool.
4. WHERE the PowerShell wrapper is available, WHEN the CLI tool completes with exit code 0, THE wrapper SHALL display the exit status and the full output file path.
5. WHERE the PowerShell wrapper is available, IF the CLI tool completes with a non-zero exit code, THEN THE wrapper SHALL display the exit code and an error message indicating that generation failed.

### Requirement 16: Sidecar JSON Parsing and Pretty-Printing

**User Story:** As a developer, I want robust parsing and pretty-printing of Sidecar_JSON files, so that tooling can reliably read and write tileset metadata without data loss.

#### Acceptance Criteria

1. WHEN a valid Sidecar_JSON file is provided, THE JSON Parser SHALL produce an in-memory tileset representation containing all defined tiles with Source_Rects, names, and Adjacency_Rules.
2. THE JSON Pretty_Printer SHALL format tileset data back into a valid Sidecar_JSON file indented with 2-space pretty-printing as produced by picojson's serialize(true).
3. WHEN a tileset object is parsed, pretty-printed, and parsed again, THE JSON Parser SHALL produce a tileset object where every tile has identical Source_Rect values, name strings, and Adjacency_Rule entries as the original, regardless of key ordering in the serialized output.
4. IF a Sidecar_JSON contains unknown fields, THEN THE JSON Parser SHALL store those fields in the in-memory representation and THE JSON Pretty_Printer SHALL emit them in the output with their original values and types preserved, though key order may differ due to alphabetical sorting.
5. IF a Sidecar_JSON file contains malformed JSON, THEN THE JSON Parser SHALL reject the input without producing a partial tileset and SHALL report an error message indicating the parse failure location.

### Requirement 17: Map File Parsing and Pretty-Printing

**User Story:** As a developer, I want robust parsing and pretty-printing of Map_File JSON, so that map data survives read-write cycles through different tools without corruption.

#### Acceptance Criteria

1. WHEN a valid Map_File JSON is provided, THE JSON Parser SHALL produce an in-memory map grid representation that reflects the row count, column count, and cell values of the source JSON.
2. THE JSON Pretty_Printer SHALL format map grid data back into a valid Map_File JSON with consistent 2-space indentation such that the output is itself parseable by the JSON Parser without error.
3. THE JSON Parser and Pretty_Printer SHALL satisfy the round-trip property: for all valid map definitions, parsing the JSON into a map grid, pretty-printing that grid back to JSON, and parsing the result again SHALL produce a map grid that is cell-by-cell identical to the first parsed result.
4. IF a Map_File JSON contains unknown fields (fields other than the grid definition), THEN THE JSON Parser SHALL preserve those fields in the in-memory representation, and the Pretty_Printer SHALL emit them in the output JSON so that a parse-then-print cycle retains all original field names and values.
5. IF the provided Map_File JSON is malformed (invalid JSON syntax) or structurally invalid (grid field missing, grid is not an array of arrays, any row length does not match the first row's length, or dimensions exceed declared values), THEN THE JSON Parser SHALL return an error indication describing the failure reason and SHALL NOT produce a partial or corrupted map grid.
