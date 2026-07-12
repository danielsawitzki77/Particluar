# Particluar Code Style Guide

When writing or modifying code in this project, follow these conventions strictly.

## Folder Naming
- All lowercase, underscore-separated: `body_renderer`, `tile_renderer`, `math_lib`

## File Naming
- Source files (`.cpp`, `.h`): PascalCase — `BodyTypes.h`, `WFCGenerator.cpp`, `TestBodyLoader.cpp`
- vcxproj files: PascalCase — `TileRenderer.vcxproj`, `BodyViewer.vcxproj`

## Subsystem Prefixes
| Prefix | Domain |
|--------|--------|
| `tile_` | 2D tile/map rendering and WFC generation |
| `body_` | 3D body/shape rendering and joint system |
| `math_` | Shared math library |

## Code Identifiers

| Category | Convention | Example |
|----------|-----------|---------|
| Namespaces | PascalCase | `BodyRenderer`, `Particluar` |
| Classes / Structs | PascalCase | `WFCGenerator`, `BodyNode`, `PlacedTile` |
| Enum types | PascalCase | `ShapeType`, `WFCStatus` |
| Enum values | PascalCase (no underscores) | `FaceConnection`, `TopCap` |
| Public methods | PascalCase | `SetPosition`, `GenerateJigsaw` |
| Private methods | PascalCase | `RenderNode`, `SplitGap` |
| Member variables | `m_` + camelCase | `m_cacheValid`, `m_pivotX` |
| Struct POD fields | camelCase (no prefix) | `targetWidth`, `parentFaceIndex` |
| Function parameters | camelCase | `deltaTime`, `startRow` |
| Local variables | camelCase | `tileSize`, `numSamples` |
| Constants / defines | UPPER_SNAKE_CASE | `MAX_LIGHTS`, `DEFAULT_SEED` |
