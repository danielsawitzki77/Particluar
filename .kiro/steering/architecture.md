# Particluar Architecture

## Project Structure

Particluar is an **umbrella project** with two independent tech domains:

| Domain | Library | Purpose |
|--------|---------|---------|
| 2D Tile/Map | `tile_renderer/` (TileRenderer.lib) | Tileset loading, WFC generation, streaming map, rendering |
| 3D Body | `body_renderer/` (BodyRenderer.lib) | Shape generation, joints, triangulation, GL rendering |
| Shared Math | `math_lib/` (MathLib.lib) | Vec3, Mat4, shared math types |

## Rules

### main.cpp Must Stay Minimal

The application entry point (`src/main.cpp`) must:
- Only create SDL resources (window, renderer)
- Load config
- Instantiate high-level manager objects (`TileWorld`, future `BodyWorld`)
- Run the main loop: poll events → update → render
- **Never** contain domain logic (tile placement, adjacency, generation algorithms)

If a feature requires more than 10 lines of new code in main.cpp, it belongs in a library class.

### Domain Logic Lives in Libraries

All 2D tile/map logic belongs in `tile_renderer/`:
- `TileWorld` — Top-level manager for the tile system (layers, update, render)
- `StreamingMapGenerator` — Per-layer streaming generation with adjacency tables
- `TilesetLoader` — JSON parsing, texture loading
- `TileRenderer` — SDL rendering of placed tiles
- `Camera`, `Viewport` — View management
- `WFCGenerator`, `ChunkManager` — Alternative generation strategies

All 3D body logic belongs in `body_renderer/`:
- `BodyRendererGL` — OpenGL rendering of body hierarchies
- `BodyLoader` — JSON parsing of body definitions
- `FaceGenerator`, `Triangulator`, `ConnectionSolver` — Geometry pipeline

### No Static Functions for Reusable Logic

If logic is reusable or stateful, it must be a class method. Free-standing `static` helper functions are only acceptable for:
- Truly one-off utilities within a single .cpp file (< 10 lines)
- Pure math helpers that have no state

### Class Design

- Each class has a single responsibility
- State is private with public interface methods
- Init/Load methods are separate from constructors (allow re-initialization)
- Classes that manage resources expose `const` accessors for read-only consumers

### New Features Checklist

When adding a new feature:
1. Determine which domain it belongs to (2D tile, 3D body, or shared)
2. Place it in the corresponding library (`tile_renderer/`, `body_renderer/`, `math_lib/`)
3. Add header in `<lib>/include/`, source in `<lib>/src/`
4. Add to the library's `.vcxproj`
5. Expose to main.cpp only through high-level manager classes (`TileWorld`, etc.)
6. main.cpp gets at most a one-line call to the new functionality
