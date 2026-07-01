# Implementation Plan: OpenGL Body Renderer

## Overview

Build a static library (`body_renderer/`) and viewer application (`Body_Viewer`) for the Particluar project. The library parses hierarchical 3D body JSON files into shape trees, decomposes primitives into triangulated faces, resolves parent-child connections via geometric transforms, and renders using OpenGL 2 immediate mode with Phong lighting. The viewer provides interactive rotation and model switching via SDL3.

Implementation follows the design's data-flow pipeline: JSON → BodyNode tree → connection resolution → face generation → triangulation → GL rendering.

## Tasks

- [ ] 1. Set up body_renderer library structure and core types
  - [ ] 1.1 Create body_renderer directory structure and vcxproj
    - Create `body_renderer/` directory mirroring `renderer/` layout: `include/`, `src/`, `bin/`, `obj/`
    - Create `body_renderer/BodyRenderer.vcxproj` as a static library project (C++14, MSVC, x64)
    - Add the project to `Particluar.sln`
    - Configure include paths for `vendor/picojson/`, `vendor/SDL/include/`
    - _Requirements: 1.1, 2.1, 6.1_

  - [ ] 1.2 Implement BodyTypes.h with Vec3, Mat4, ShapeParams, Connection, BodyNode
    - Create `body_renderer/include/BodyTypes.h` with all struct/enum definitions from the design
    - Include `ShapeType` enum (Box, Cone, Cylinder, Sphere, Torus, Frustum)
    - Include `ConnectionType` enum (Face_Connection, Edge_Connection, Point_Connection)
    - Include `Vec3`, `Mat4` (column-major), `ShapeParams`, `Connection`, `BodyNode` structs
    - Implement Mat4 identity helper and basic matrix multiply utility
    - _Requirements: 1.1, 2.1, 3.1_

- [ ] 2. Implement JSON parsing (BodyLoader)
  - [ ] 2.1 Implement BodyLoader with LoadFromFile, LoadFromString, and Serialize
    - Create `body_renderer/include/BodyLoader.h` and `body_renderer/src/BodyLoader.cpp`
    - Use picojson to parse JSON into BodyNode tree recursively
    - Validate all required fields per shape type; return descriptive errors on failure
    - Implement `Serialize()` to convert BodyNode tree back to JSON string
    - Handle all error cases from the design's error table (missing fields, invalid types, bad ranges)
    - _Requirements: 1.1, 1.2, 7.1_

  - [ ]* 2.2 Write property test for JSON round-trip preservation
    - **Property 1: JSON Round-Trip Preservation**
    - **Validates: Requirements 1.1, 7.1**

  - [ ]* 2.3 Write property test for invalid JSON graceful rejection
    - **Property 2: Invalid JSON Graceful Rejection**
    - **Validates: Requirements 1.2**

- [ ] 3. Implement face generation (FaceGenerator)
  - [ ] 3.1 Implement FaceGenerator for all 6 primitive types
    - Create `body_renderer/include/FaceGenerator.h` and `body_renderer/src/FaceGenerator.cpp`
    - Implement `GenerateBox`: 6 quad faces with outward normals
    - Implement `GenerateCone`: segments side triangles + 1 base polygon
    - Implement `GenerateCylinder`: segments side quads + 2 cap polygons
    - Implement `GenerateSphere`: lat_segments × lon_segments quad faces
    - Implement `GenerateTorus`: ring_segments × side_segments quad faces
    - Implement `GenerateFrustum`: segments side quads + 2 cap polygons
    - All faces use CCW vertex ordering with unit-length outward normals
    - _Requirements: 2.1, 6.1_

  - [ ]* 3.2 Write property test for face count invariant
    - **Property 3: Face Count Invariant**
    - **Validates: Requirements 2.1**

  - [ ]* 3.3 Write property test for face normal unit-length and perpendicularity
    - **Property 5: Face Normal Unit-Length and Perpendicularity**
    - **Validates: Requirements 6.1**

- [ ] 4. Implement triangulation (Triangulator)
  - [ ] 4.1 Implement Triangulator with fan decomposition
    - Create `body_renderer/include/Triangulator.h` and `body_renderer/src/Triangulator.cpp`
    - Implement fan decomposition: N-vertex face → N-2 triangles
    - Each triangle inherits the source face's normal (flat shading)
    - Implement both `Triangulate(faces)` and `TriangulateFace(face)` methods
    - _Requirements: 2.2_

  - [ ]* 4.2 Write property test for triangle decomposition validity
    - **Property 4: Triangle Decomposition Validity**
    - **Validates: Requirements 2.2**

- [ ] 5. Checkpoint - Verify core geometry pipeline
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 6. Implement connection resolution (ConnectionSolver)
  - [ ] 6.1 Implement ConnectionSolver with Face, Edge, and Point connection types
    - Create `body_renderer/include/ConnectionSolver.h` and `body_renderer/src/ConnectionSolver.cpp`
    - Implement `ResolveTree()` for recursive depth-first transform resolution
    - Implement `ComputeFaceConnection`: position child origin on parent face plane using offset_u/offset_v, apply rotation around connection normal
    - Implement `ComputeEdgeConnection`: position child origin on parent edge line segment
    - Implement `ComputePointConnection`: position child origin at parent vertex
    - Validate face indices against generated face count; return identity on out-of-range
    - _Requirements: 3.1, 3.2_

  - [ ]* 6.2 Write property test for connection transform geometric correctness
    - **Property 6: Connection Transform Geometric Correctness**
    - **Validates: Requirements 3.1**

  - [ ]* 6.3 Write property test for nested transform accumulation
    - **Property 7: Nested Transform Accumulation**
    - **Validates: Requirements 3.2**

- [ ] 7. Implement OpenGL rendering (BodyRenderer and LightManager)
  - [ ] 7.1 Implement LightManager for OpenGL fixed-function lighting
    - Create `body_renderer/include/LightManager.h` and `body_renderer/src/LightManager.cpp`
    - Implement `Apply()`: configure GL_LIGHT0..GL_LIGHT7 with position, diffuse, specular, attenuation
    - Set global ambient via `glLightModelfv`
    - Implement `Disable()`: disable all GL lights and GL_LIGHTING
    - Cap at 8 lights; log warning via SDL_Log if more requested
    - _Requirements: 6.1_

  - [ ] 7.2 Implement BodyRenderer with recursive tree traversal and immediate-mode GL submission
    - Create `body_renderer/include/BodyRenderer.h` and `body_renderer/src/BodyRenderer.cpp`
    - Implement `Render()`: set up lighting via LightManager, begin recursive render from root
    - Implement `RenderNode()`: multiply parent_world × node local_transform, generate faces, triangulate, submit
    - Implement `SubmitTriangles()`: use `glBegin(GL_TRIANGLES)` with `glNormal3f` and `glVertex3f` per vertex
    - Apply world transform via `glPushMatrix`/`glMultMatrixf`/`glPopMatrix`
    - Check for valid GL context before rendering; log and return on failure
    - _Requirements: 4.1, 6.1_

- [ ] 8. Implement ModelSwitcher utility
  - [ ] 8.1 Implement ModelSwitcher for directory scanning and cycling
    - Create `body_renderer/include/ModelSwitcher.h` and `body_renderer/src/ModelSwitcher.cpp`
    - Implement `LoadDirectory()`: scan for `.json` files, sort alphabetically, return false if empty
    - Implement `Next()`/`Previous()` with wraparound (index N-1 → 0, index 0 → N-1)
    - Implement `GetCurrentPath()`, `GetCount()`, `GetCurrentIndex()`
    - _Requirements: 5.3_

  - [ ]* 8.2 Write property test for model index wraparound
    - **Property 8: Model Index Wraparound**
    - **Validates: Requirements 5.3**

- [ ] 9. Checkpoint - Verify library compiles and all property tests pass
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 10. Implement Body_Viewer application
  - [ ] 10.1 Create Body_Viewer vcxproj and main application loop
    - Create `Body_Viewer/` directory with `Body_Viewer.vcxproj` as a console application
    - Add to `Particluar.sln`, link against `body_renderer.lib` and SDL3
    - Implement `main()`: SDL_Init, create window with SDL_WINDOW_OPENGL flag, create GL context
    - Set up perspective projection via `gluPerspective` or manual matrix
    - Implement event loop: poll SDL events, handle quit
    - _Requirements: 4.1, 5.1_

  - [ ] 10.2 Implement orbit camera with WASD rotation controls
    - Add orbit camera state (yaw, pitch, distance) in the viewer
    - Handle SDL_EVENT_KEY_DOWN for W/A/S/D to rotate the model (adjust yaw/pitch)
    - Apply camera transform via `gluLookAt` or manual view matrix each frame
    - _Requirements: 5.1, 5.2_

  - [ ] 10.3 Wire model loading and switching with arrow keys
    - On startup, load body directory path from command-line args (default: `./bodies/`)
    - Use ModelSwitcher to scan the directory
    - Handle Left/Right arrow keys to call Previous()/Next() and reload the current model
    - Display current model name in window title via `SDL_SetWindowTitle`
    - _Requirements: 5.3_

  - [ ] 10.4 Wire full render pipeline in the frame loop
    - Each frame: clear buffers, apply camera, call BodyRenderer::Render() with default RenderParams
    - Set up default lighting: one white point light at (5,5,5), ambient (0.2,0.2,0.2)
    - Set model color to a neutral grey, shininess = 32.0
    - Call `SDL_GL_SwapWindow` to present
    - _Requirements: 4.1, 6.1_

- [ ] 11. Set up test project (body_renderer_tests)
  - [ ] 11.1 Create body_renderer_tests vcxproj linking rapidcheck and body_renderer
    - Create `body_renderer_tests/` directory with vcxproj
    - Link against `body_renderer.lib` and `vendor/rapidcheck/`
    - Add all property test source files (one .cpp per component)
    - Configure as console app outputting test results
    - _Requirements: 7.1_

  - [ ] 11.2 Implement RapidCheck generators for ShapeParams, BodyNode, and Connection
    - Create custom `rc::gen::Arbitrary` specializations for ShapeParams (valid ranges per type)
    - Create generator for Connection (valid type, valid face indices for a given shape)
    - Create generator for BodyNode trees (configurable max depth, 0-3 children per node)
    - Ensure generated values satisfy validation constraints (segments >= 3, radius > 0, etc.)
    - _Requirements: 7.1_

- [ ] 12. Create sample body JSON files
  - [ ] 12.1 Create example body JSON files for testing the viewer
    - Create `bodies/` directory at project root
    - Create `bodies/unit_cube.json`: single Box node (1×1×1)
    - Create `bodies/robot_arm.json`: Cylinder root with Sphere child via Face_Connection (from design example)
    - Create `bodies/snowman.json`: 3 stacked Spheres using Face_Connection
    - _Requirements: 1.1, 3.1_

- [ ] 13. Final checkpoint - Full integration verification
  - Ensure all tests pass, ask the user if questions arise.

## Notes

- Tasks marked with `*` are optional and can be skipped for faster MVP
- Each task references specific requirements for traceability
- Checkpoints ensure incremental validation
- Property tests validate universal correctness properties from the design (8 properties total)
- The library mirrors the existing `renderer/` static library structure in the project
- All code targets C++14, MSVC, Windows x64
- picojson is already available at `vendor/picojson/`
- RapidCheck is already available at `vendor/rapidcheck/`
- SDL3 is already available at `vendor/SDL/`

## Task Dependency Graph

```json
{
  "waves": [
    { "id": 0, "tasks": ["1.1", "1.2"] },
    { "id": 1, "tasks": ["2.1", "3.1", "4.1"] },
    { "id": 2, "tasks": ["2.2", "2.3", "3.2", "3.3", "4.2", "6.1", "8.1"] },
    { "id": 3, "tasks": ["6.2", "6.3", "7.1", "8.2"] },
    { "id": 4, "tasks": ["7.2", "11.1"] },
    { "id": 5, "tasks": ["11.2", "10.1", "12.1"] },
    { "id": 6, "tasks": ["10.2", "10.3"] },
    { "id": 7, "tasks": ["10.4"] }
  ]
}
```
