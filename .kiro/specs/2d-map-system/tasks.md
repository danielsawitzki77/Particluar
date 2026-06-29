# Implementation Plan

## Overview
Implementation of the 2D Map System module for the Particluar project. Tasks are ordered by dependency — foundational components first (project scaffolding, JSON utilities), then data structures, rendering, map generation, CLI tools, and finally web tooling.

## Tasks

- [ ] 1. Project Scaffolding — Renderer Static Library (Req 1)
  - [ ] 1.1 Create `renderer/` directory with `include/` and `src/` subdirectories
  - [ ] 1.2 Create `renderer/Renderer.vcxproj` as a static library (.lib) targeting x64 Debug and Release, C++14 standard
  - [ ] 1.3 Configure Renderer.vcxproj include paths for SDL3, SDL_image, and picojson from `vendor/`
  - [ ] 1.4 Add Renderer.vcxproj to `Particluar.sln` as a project reference
  - [ ] 1.5 Update `Particluar.vcxproj` to reference Renderer.lib and add `renderer/include/` to its additional include directories
  - [ ] 1.6 Verify the solution builds with an empty Renderer.lib (no source files yet) and Particluar.exe links successfully

- [ ] 2. JSON Utilities (Reqs 16, 17)
  - [ ] 2.1 Create `renderer/include/JsonUtil.h` with the `JsonUtil` namespace declaring ParseFile, ParseString, Serialize, WriteFile, and safe field extraction helpers (GetInt, GetDouble, GetString, GetBool, GetArray, GetObject)
  - [ ] 2.2 Implement `renderer/src/JsonUtil.cpp` — ParseFile reads entire file into string and delegates to ParseString
  - [ ] 2.3 Implement ParseString wrapping `picojson::parse` with error logging via SDL_Log
  - [ ] 2.4 Implement Serialize using `picojson::value::serialize(true)` for 2-space pretty-printing
  - [ ] 2.5 Implement WriteFile with proper file I/O error handling
  - [ ] 2.6 Implement safe extraction helpers — each checks `obj.count(key)` and `is<T>()` before `get<T>()`
  - [ ] 2.7 Add JsonUtil.cpp to Renderer.vcxproj and confirm the library compiles

- [ ] 3. Global Configuration (Req 2)
  - [ ] 3.1 Create `renderer/include/GlobalConfig.h` with GlobalConfigData struct and GlobalConfig class
  - [ ] 3.2 Implement `renderer/src/GlobalConfig.cpp` — ApplyDefaults with hard-coded values (tile 32x32, viewport 800x600 at 0,0, scroll_speed 200.0)
  - [ ] 3.3 Implement Load: parse JSON, extract and validate all fields against specified ranges
  - [ ] 3.4 Implement all-or-nothing validation: if any field fails, reject file, apply defaults, log reason via SDL_Log
  - [ ] 3.5 Implement Serialize for round-trip property
  - [ ] 3.6 Add GlobalConfig.cpp to Renderer.vcxproj and confirm build

- [ ] 4. Tileset Data Structures and Sidecar JSON Parsing (Reqs 6, 16)
  - [ ] 4.1 Create `renderer/include/TilesetLoader.h` with SourceRect, AdjacencyRules, TileDef, Tileset, TilesetDef structs and TilesetLoader class
  - [ ] 4.2 Implement `renderer/src/TilesetLoader.cpp` — ParseSidecarJson: parse root object, extract tiles array
  - [ ] 4.3 For each tile: extract id, source_rect, adjacency with validation; skip invalid entries with logged warnings
  - [ ] 4.4 Validate Source_Rect fits within PNG dimensions; build id→index map
  - [ ] 4.5 Implement LoadTilesetDef (data-only, no SDL_Texture) for CLI tools
  - [ ] 4.6 Preserve unknown fields for round-trip fidelity
  - [ ] 4.7 Add TilesetLoader.cpp to Renderer.vcxproj and confirm build

- [ ] 5. Tileset Loading with SDL_Texture (Req 7)
  - [ ] 5.1 Implement LoadTileset: call IMG_LoadTexture for PNG, query dimensions, parse sidecar JSON
  - [ ] 5.2 Handle load failures: return false and log via SDL_Log on PNG or JSON error
  - [ ] 5.3 Skip individual invalid tile entries while continuing to process remaining tiles
  - [ ] 5.4 Create a test tileset in `assets/tilesets/test/` with small PNG and valid sidecar JSON

- [ ] 6. Map File Format — Parsing and Serialization (Reqs 11, 17)
  - [ ] 6.1 Create `renderer/include/MapLoader.h` with MapData struct and MapLoader class
  - [ ] 6.2 Implement LoadMap: parse JSON, extract width/height/tileset/grid, validate dimensions
  - [ ] 6.3 Implement SerializeMap with 2-space pretty-printed JSON output
  - [ ] 6.4 Implement SaveMap and ValidateAgainstTileset
  - [ ] 6.5 Preserve unknown fields and ensure round-trip property
  - [ ] 6.6 Add MapLoader.cpp to Renderer.vcxproj and confirm build

- [ ] 7. Viewport and Camera System (Reqs 3, 4)
  - [ ] 7.1 Create `renderer/include/Viewport.h` and `renderer/include/Camera.h` with respective classes
  - [ ] 7.2 Implement Viewport: SetRect, IsValid, ComputeVisibleTiles, ApplyClip/RemoveClip via SDL_SetRenderClipRect
  - [ ] 7.3 Implement Camera: position (float x,y), pivot (normalized 0-1, clamped), Update with WASD + deltaTime
  - [ ] 7.4 Add Viewport.cpp and Camera.cpp to Renderer.vcxproj and confirm build

- [ ] 8. Tile Renderer (Req 3)
  - [ ] 8.1 Create `renderer/include/TileRenderer.h` with TileRenderer class
  - [ ] 8.2 Implement RenderLayer: iterate visible tile range, resolve IDs, render via SDL_RenderTexture with source rects
  - [ ] 8.3 Apply alpha modulation and render magenta fallback for unresolved tile IDs
  - [ ] 8.4 Skip rendering when viewport is invalid; draw in ascending layer order
  - [ ] 8.5 Add TileRenderer.cpp to Renderer.vcxproj and confirm build

- [ ] 9. WASD Camera Scrolling PoC — Particluar App Integration (Req 5)
  - [ ] 9.1 Update `src/main.cpp` to create SDL_Window, SDL_Renderer, and instantiate all Renderer components
  - [ ] 9.2 Load GlobalConfig, test tileset, and test map; implement main loop with event polling and delta time
  - [ ] 9.3 Call Camera::Update each frame with keyboard state and scroll_speed from config
  - [ ] 9.4 Call Viewport::ComputeVisibleTiles and TileRenderer::RenderLayer each frame
  - [ ] 9.5 Verify WASD scrolling, diagonal movement, and scroll_speed config/fallback

- [ ] 10. WFC Generator Core Algorithm (Req 8)
  - [ ] 10.1 Create `renderer/include/WFCGenerator.h` with WFCStatus, WFCParams, WFCResult, WFCGenerator class
  - [ ] 10.2 Implement Generate: validate inputs, initialize grid with all possibilities
  - [ ] 10.3 Implement SelectMinEntropy, Collapse (std::mt19937), and Propagate (BFS constraint propagation)
  - [ ] 10.4 Main loop until all collapsed or contradiction; build MapData on success
  - [ ] 10.5 Verify determinism with fixed seed and zero SDL dependencies
  - [ ] 10.6 Add WFCGenerator.cpp to Renderer.vcxproj and confirm build

- [ ] 11. Real-Time Map Generation Integration (Req 9)
  - [ ] 11.1 Add G-key binding in app main loop to trigger WFC generation at runtime
  - [ ] 11.2 On success replace active MapData and render on next frame; on contradiction keep previous map
  - [ ] 11.3 Verify generation completes within 5 seconds for 64x64 grid

- [ ] 12. Offline CLI Map Generator Tool (Req 10)
  - [ ] 12.1 Create `tools/mapgen/` with main.cpp and mapgen.vcxproj (Console App linking Renderer.lib); add to solution
  - [ ] 12.2 Implement CLI argument parsing: --tileset, --output, --width, --height, --seed (optional)
  - [ ] 12.3 Implement validation, tileset loading, WFC generation, and map file writing with proper error handling and exit codes
  - [ ] 12.4 Verify CLI builds and produces valid Map_File JSON

- [ ] 13. Tile Naming CLI Tool (Req 12)
  - [ ] 13.1 Create `tools/tilename/` with main.cpp and tilename.vcxproj (Console App linking Renderer.lib); add to solution
  - [ ] 13.2 Implement: parse sidecar, identify unnamed tiles, assign deterministic names without collision, write back in place
  - [ ] 13.3 Handle already-all-named case (exit 0, no modification) and error cases (exit non-zero)

- [ ] 14. Web Editor — Project Setup and Tileset Configurator (Req 13)
  - [ ] 14.1 Create `tools/web-editor/` with package.json, tsconfig.json, and Node.js server setup
  - [ ] 14.2 Implement API endpoints for listing tilesets, reading PNG/JSON, and writing updated sidecar JSON
  - [ ] 14.3 Implement Tileset Configurator UI: tileset browser, PNG with grid overlay, grid parameter controls, tile selection with adjacency rule editing, save functionality

- [ ] 15. Web Editor — Level Editor (Req 14)
  - [ ] 15.1 Implement Level Editor UI: grid dimension input, tile palette, grid canvas
  - [ ] 15.2 Implement paint/erase mechanics and Map_File JSON export with validation
  - [ ] 15.3 Handle edge cases: no tileset loaded, no tile selected

- [ ] 16. PowerShell GUI Wrapper (Req 15) [optional]
  - [ ] 16.1 Create `scripts/mapgen_gui.ps1` with Windows Forms dialog containing input fields and Generate button
  - [ ] 16.2 Implement validation, CLI invocation, and result display

## Task Dependency Graph

```json
{
  "waves": [
    [1],
    [2],
    [3],
    [4],
    [5, 10, 13],
    [6],
    [7, 12, 14],
    [8, 15, 16],
    [9, 11]
  ]
}
```

## Notes
- Tasks 1–9 form the critical path for the rendering PoC
- Task 10 (WFC) depends only on tileset data structures (Task 4)
- CLI tools (12, 13) can be developed in parallel with rendering (7–9)
- Web editor (14, 15) can be developed independently after Task 4
- Task 16 (PowerShell wrapper) is optional and depends on Task 12
