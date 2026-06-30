# Requirements Document

## Introduction

An independent OpenGL-based 3D body renderer module for the Particluar project. The module renders flat-shaded, topologically closed 3D bodies composed of connected geometric primitives (boxes, cones, cylinders), defined via a JSON-based model format. The renderer uses legacy OpenGL 2 with Phong lighting, routed through SDL3 for window and OpenGL context management. A proof-of-concept viewer application demonstrates the renderer by loading and displaying models from a directory with keyboard-driven navigation and rotation.

## Glossary

- **Body_Renderer**: The static library (.lib) within Particluar.sln responsible for OpenGL 2 rendering of flat-shaded 3D bodies with Phong lighting.
- **Body**: A complete 3D model defined as an acyclic tree of connected geometric primitives, loaded from a Body_JSON file.
- **Body_JSON**: A JSON file describing a Body as a shape tree of primitives with connection parameters.
- **Shape_Tree**: An acyclic tree data structure where each node is a geometric primitive connected to its parent at a specified attachment point.
- **Primitive**: A basic geometric solid used as a building block in a Body. Supported types: Box, Cone, Cylinder.
- **Box**: A rectangular parallelepiped primitive with configurable width, height, and depth dimensions.
- **Cone**: A conical primitive with configurable base radius, height, and lateral resolution (number of sides for tessellation).
- **Cylinder**: A cylindrical primitive with configurable radius, height, and lateral resolution (number of sides for tessellation).
- **Connection**: The attachment relationship between a child primitive and its parent in the Shape_Tree, defined by the parent's face, the child's face, and a rotation_position parameter.
- **Rotation_Position**: A floating-point parameter in the range 0.0 to 1.0 that defines the rotational orientation of a child primitive relative to its parent's connection face. 0.0 and 1.0 represent the same orientation (full rotation wraps).
- **Face**: A flat polygon resulting from decomposing a primitive's surface. Faces can be triangles, quads, or N-gons depending on the primitive type.
- **Face_Decomposition**: The process of converting a Body's primitives into a list of planar polygon faces with computed normals.
- **Triangle_Decomposition**: The process of subdividing polygon faces into triangles suitable for OpenGL rendering.
- **Phong_Lighting**: The illumination model combining ambient, diffuse, and specular components computed per-face (flat shading).
- **Point_Light**: A light source defined by a 3D position and color (RGB), contributing to the Phong_Lighting calculation.
- **Flat_Shading**: A rendering technique where each triangle face receives a single uniform color computed from the face normal, with no interpolation across vertices.
- **Body_Viewer**: The proof-of-concept executable that loads Body_JSON files from a directory and renders them with interactive rotation and model switching.
- **Lateral_Resolution**: The integer number of sides used to approximate curved surfaces (Cone, Cylinder) as polygon meshes. Higher values produce smoother approximations.

## Requirements

### Requirement 1: Body Renderer Static Library Integration

**User Story:** As a developer, I want the Body_Renderer to be a static library within the existing Particluar.sln, so that the 3D rendering module builds as part of the solution independently from the existing tilemap code.

#### Acceptance Criteria

1. THE Body_Renderer SHALL compile as a static library (.lib) project within Particluar.sln targeting x64 Debug and Release configurations, using the C++14 language standard, and SHALL output the library file to the solution's shared bin\$(Configuration)\ directory.
2. THE Body_Renderer SHALL have zero compile-time dependencies on Particluar application source files (src/ and include/App.h) and zero dependencies on the existing 2D map Renderer library.
3. THE Body_Renderer SHALL depend only on SDL3 headers (from the solution's vendor\SDL\include path) for OpenGL context and window management, on picojson (from the solution's vendor\picojson path) for JSON parsing, and on OpenGL headers provided by the Windows platform SDK (OpenGL32.lib).
4. WHEN the Body_Viewer application project references the Body_Renderer library, THE linker SHALL resolve all Body_Renderer symbols without additional library dependencies beyond SDL3, picojson, OpenGL32.lib, and Windows platform libraries already required by SDL3 static linking.
5. THE Body_Renderer project SHALL reside in a dedicated subdirectory (body_renderer/) within the Particluar repository root and SHALL NOT require files from outside the repository to compile.
6. THE Body_Renderer project SHALL expose a public include directory (body_renderer/include/) containing all headers required by consuming projects, so that a consumer adding this single include path can compile against the Body_Renderer API without referencing its internal source files.

### Requirement 2: OpenGL 2 Rendering Pipeline

**User Story:** As a developer, I want the renderer to use legacy OpenGL 2 with individual triangle submission, so that the implementation remains straightforward and compatible with basic hardware.

#### Acceptance Criteria

1. THE Body_Renderer SHALL set SDL_GL_CONTEXT_MAJOR_VERSION to 2, SDL_GL_CONTEXT_MINOR_VERSION to 1, and SDL_GL_CONTEXT_PROFILE_MASK to SDL_GL_CONTEXT_PROFILE_COMPATIBILITY via SDL_GL_SetAttribute before creating an OpenGL context via SDL3's SDL_GL_CreateContext.
2. THE Body_Renderer SHALL submit geometry to OpenGL as individual triangles using glBegin(GL_TRIANGLES)/glEnd() or equivalent immediate-mode calls, without using triangle strips or indexed buffers.
3. THE Body_Renderer SHALL set per-triangle face normals via glNormal3f before emitting the triangle's vertices, producing flat-shaded appearance.
4. THE Body_Renderer SHALL enable depth testing (GL_DEPTH_TEST) so that overlapping triangles are resolved correctly by depth.
5. THE Body_Renderer SHALL clear the color buffer to black (RGBA 0.0, 0.0, 0.0, 1.0) and clear the depth buffer to 1.0 at the start of each frame before rendering any geometry.
6. IF SDL3 fails to create an OpenGL 2.1-compatible context, THEN THE Body_Renderer SHALL log an error via SDL_Log indicating the failure reason and return an initialization failure status.

### Requirement 3: Phong Lighting Model

**User Story:** As a developer, I want the renderer to support the full Phong lighting model with configurable point lights, so that bodies appear with realistic ambient, diffuse, and specular illumination.

#### Acceptance Criteria

1. THE Body_Renderer SHALL support an arbitrary but fixed number of point light sources, configurable at compile time via a constant (minimum 1, maximum 8).
2. THE Body_Renderer SHALL configure each Point_Light with a 3D position (x, y, z as floating-point), diffuse color (RGB, each component 0.0 to 1.0), and specular color (RGB, each component 0.0 to 1.0).
3. THE Body_Renderer SHALL apply ambient lighting as a global RGB color (each component 0.0 to 1.0) added uniformly to all faces regardless of orientation.
4. THE Body_Renderer SHALL compute diffuse lighting per face using the dot product of the outward-facing face normal and the unit vector from the face toward the light source, clamped to a minimum of 0.0.
5. THE Body_Renderer SHALL compute specular lighting per face using the Phong reflection model with a configurable shininess exponent (floating-point, range 1.0 to 128.0), where the view direction is defined as the vector from the face toward the camera position.
6. THE Body_Renderer SHALL configure OpenGL fixed-function lighting via glLightfv and glMaterialfv to achieve the Phong model, enabling GL_LIGHTING and the appropriate GL_LIGHTn constants.
7. IF no lights are enabled, THEN THE Body_Renderer SHALL render all geometry using only the ambient color component.
8. THE Body_Renderer SHALL clamp the final per-face color (sum of ambient, diffuse, and specular contributions across all enabled lights) to the range 0.0 to 1.0 per RGB component before rendering.
9. IF a shininess exponent is set to a value outside the range 1.0 to 128.0, THEN THE Body_Renderer SHALL clamp the value to the nearest bound (1.0 or 128.0) and use the clamped value for specular computation.

### Requirement 4: Body JSON Data Format

**User Story:** As a content creator, I want a JSON-based format to define 3D bodies as trees of connected primitives, so that models are human-readable, easy to author, and tooling-friendly.

#### Acceptance Criteria

1. THE Body_JSON SHALL be a JSON object with a root-level "name" field (string, 1 to 128 characters) and a "root" field containing the root primitive node of the Shape_Tree.
2. THE Body_JSON SHALL define each primitive node as a JSON object containing: "type" (string: "box", "cone", or "cylinder"), "dimensions" (object with type-specific size parameters), "color" (object with "r", "g", "b" fields, each 0.0 to 1.0), and an optional "children" array of child connection objects. IF the "children" field is absent or is an empty array, THEN THE Body_JSON Parser SHALL treat the node as a leaf node with zero children.
3. THE Body_JSON SHALL define Box dimensions as: "width" (float, 0.001 to 1000.0), "height" (float, 0.001 to 1000.0), "depth" (float, 0.001 to 1000.0).
4. THE Body_JSON SHALL define Cone dimensions as: "radius" (float, 0.001 to 1000.0), "height" (float, 0.001 to 1000.0), "sides" (integer, 3 to 128).
5. THE Body_JSON SHALL define Cylinder dimensions as: "radius" (float, 0.001 to 1000.0), "height" (float, 0.001 to 1000.0), "sides" (integer, 3 to 128).
6. THE Body_JSON SHALL define each child connection object as containing: "parent_face" (string identifying the attachment face on the parent), "child_face" (string identifying the attachment face on the child), "rotation_position" (float, 0.0 to 1.0), and "node" (the child primitive node object).
7. THE Shape_Tree encoded in Body_JSON SHALL be acyclic and SHALL have a maximum nesting depth of 32 levels; IF a node references itself, creates a cycle through its children, or exceeds the maximum depth, THEN THE Body_JSON Parser SHALL reject the file and report an error indicating the structural violation.
8. IF a primitive node is missing any required field ("type", "dimensions", or "color"), THEN THE Body_JSON Parser SHALL reject the file and report an error identifying the missing field and its location in the tree.

### Requirement 5: Body JSON Parsing and Pretty-Printing

**User Story:** As a developer, I want robust parsing and serialization of Body_JSON files, so that model data survives read-write cycles without corruption.

#### Acceptance Criteria

1. WHEN a valid Body_JSON file is provided, THE Body_JSON Parser SHALL produce an in-memory Shape_Tree representation containing all primitive nodes with their types, dimensions, colors, and connection parameters.
2. THE Body_JSON Pretty_Printer SHALL format a Shape_Tree back into a valid Body_JSON file with 2-space indentation as produced by picojson's serialize(true), terminated by a single newline character.
3. FOR ALL valid Body objects, parsing the Body_JSON, pretty-printing the result, and parsing again SHALL produce a Shape_Tree where primitive types, connection face identifiers, rotation_position values, and tree structure are identical, and where all floating-point values (dimensions, color components) are equal within a tolerance of 1e-9 (round-trip property).
4. IF a Body_JSON file contains malformed JSON, THEN THE Body_JSON Parser SHALL reject the input and report an error message indicating the line number and the surrounding text where parsing failed, as provided by picojson's error string.
5. IF a Body_JSON file contains a primitive with an unrecognized "type" value, THEN THE Body_JSON Parser SHALL reject the file and report an error identifying the invalid type string and the node's path from the root (e.g., "root.children[0].node.children[1].node").
6. IF a Body_JSON file contains a dimension value that is not a positive finite number, or a "sides" value outside the range 3 to 128, THEN THE Body_JSON Parser SHALL reject the file and report an error identifying the invalid field.
7. IF a Body_JSON file contains a color component outside the range 0.0 to 1.0, THEN THE Body_JSON Parser SHALL clamp the value to the nearest bound (0.0 or 1.0) and log a warning identifying the field name and the original value.
8. IF a Body_JSON file is missing a required field ("name", "root", "type", "dimensions", "color", or any type-specific dimension key), THEN THE Body_JSON Parser SHALL reject the file and report an error identifying the missing field name and the node path where it was expected.
9. IF a Body_JSON file exceeds a nesting depth of 50 levels, THEN THE Body_JSON Parser SHALL reject the file and report an error indicating the maximum depth was exceeded.

### Requirement 6: Primitive Face Decomposition

**User Story:** As a developer, I want each primitive type to decompose into polygon faces with computed normals, so that the geometry pipeline can convert any Body into renderable triangles.

#### Acceptance Criteria

1. WHEN a Box primitive is decomposed, THE Face_Decomposition SHALL produce exactly 6 quadrilateral faces (one per face of the rectangular parallelepiped), each with an outward-pointing normal and vertices in counter-clockwise winding order as seen from outside the solid.
2. WHEN a Cone primitive with N sides is decomposed where N is at least 3, THE Face_Decomposition SHALL produce N triangular lateral faces, 1 N-gon base face, and each face SHALL have an outward-pointing normal with vertices in counter-clockwise winding order as seen from outside the solid.
3. WHEN a Cylinder primitive with N sides is decomposed where N is at least 3, THE Face_Decomposition SHALL produce N quadrilateral lateral faces, 1 N-gon top cap face, and 1 N-gon bottom cap face, each with an outward-pointing normal and vertices in counter-clockwise winding order as seen from outside the solid.
4. THE Face_Decomposition SHALL compute each face normal as the cross product of two consecutive edge vectors of that face, oriented so that the normal points away from the primitive's solid interior (consistent with the counter-clockwise vertex winding).
5. WHEN a child primitive is decomposed, THE Face_Decomposition SHALL first apply the Connection's translation (derived from parent_face and child_face center positions) and then apply the rotation (derived from rotation_position) to position the child relative to its parent, before computing the child's face vertices and normals.
6. THE Face_Decomposition SHALL produce a topologically closed mesh for each primitive, meaning every edge is shared by exactly 2 faces and the union of all face edges forms a closed surface with no gaps or dangling edges.
7. IF a Cone or Cylinder primitive is specified with N less than 3, THEN THE Face_Decomposition SHALL reject the primitive and produce no faces.

### Requirement 7: Triangle Decomposition

**User Story:** As a developer, I want polygon faces to be subdivided into triangles for OpenGL rendering, so that all geometry is expressed as triangles regardless of the original face shape.

#### Acceptance Criteria

1. WHEN a triangular face (3 vertices) is provided, THE Triangle_Decomposition SHALL output that face unchanged as a single triangle.
2. WHEN a quadrilateral face (4 vertices) is provided, THE Triangle_Decomposition SHALL subdivide it into exactly 2 triangles sharing the original face normal.
3. WHEN an N-gon face with N greater than 4 is provided, THE Triangle_Decomposition SHALL subdivide it into exactly (N - 2) triangles using fan triangulation from the first vertex, each sharing the original face normal.
4. THE Triangle_Decomposition SHALL preserve the winding order of the original face vertices such that all output triangles have consistent front-face orientation (counter-clockwise when viewed from outside the solid).
5. THE Triangle_Decomposition SHALL produce triangles whose combined area equals the area of the original convex polygon face (no gaps, no overlaps).
6. IF a face with fewer than 3 vertices is provided, THEN THE Triangle_Decomposition SHALL produce zero triangles and discard the degenerate face.

### Requirement 8: Connection System

**User Story:** As a content creator, I want primitives to connect to each other at specified faces with a rotation parameter, so that complex articulated bodies can be assembled from simple shapes.

#### Acceptance Criteria

1. THE Connection System SHALL support attaching a child primitive to any named face of its parent primitive, where Box faces are named "top", "bottom", "front", "back", "left", "right"; Cone faces are named "base" and "side"; and Cylinder faces are named "top", "bottom", and "side".
2. WHEN a child is connected to a parent face, THE Connection System SHALL position the child such that the child's specified attachment face center is coincident with the parent's specified attachment face center, and the outward-facing normal of the child's attachment face is anti-parallel (pointing in the opposite direction) to the outward-facing normal of the parent's attachment face.
3. WHEN a rotation_position value is applied, THE Connection System SHALL rotate the child around the outward-facing normal axis of the parent's attachment face by (rotation_position * 360) degrees before positioning, where rotation_position is a floating-point value in the range 0.0 to 1.0 inclusive and values outside this range SHALL be wrapped into the valid range using modulo-1.0 arithmetic (e.g., 1.5 becomes 0.5, -0.25 becomes 0.75).
4. THE Connection System SHALL support connecting any primitive type to any face of any other primitive type without restriction (Box-to-Cone, Cylinder-to-Box, Cone-to-Cone, etc.).
5. IF a Body_JSON specifies a parent_face name that does not exist on the parent primitive type, THEN THE Body_JSON Parser SHALL reject the file and report an error identifying the invalid face name.
6. IF a Body_JSON specifies a child_face name that does not exist on the child primitive type, THEN THE Body_JSON Parser SHALL reject the file and report an error identifying the invalid face name.
7. WHEN rotation_position is exactly 0.0 or exactly 1.0, THE Connection System SHALL produce identical child orientations (full rotation wraps to start).
8. WHEN a "side" face is specified as an attachment face on a Cone or Cylinder, THE Connection System SHALL treat the attachment point as the center of the curved surface's bounding extent along the height axis, with the outward-facing normal pointing radially outward perpendicular to the primitive's height axis.

### Requirement 9: Body Viewer Application

**User Story:** As a developer, I want a proof-of-concept viewer application that loads and displays Body_JSON models from a directory with keyboard controls, so that the rendering pipeline can be validated visually.

#### Acceptance Criteria

1. THE Body_Viewer SHALL build as a separate executable project within Particluar.sln, linking against the Body_Renderer static library.
2. WHEN launched, THE Body_Viewer SHALL accept an optional command-line argument specifying the directory path to scan for Body_JSON files; IF no argument is provided, THEN THE Body_Viewer SHALL default to the relative path "assets/bodies/".
3. WHEN the directory is scanned, THE Body_Viewer SHALL enumerate all files with a ".json" extension, sort them in case-insensitive lexicographic order by filename, and attempt to parse each as a Body_JSON file, building an ordered list of successfully loaded Bodies.
4. WHEN at least one valid Body is loaded, THE Body_Viewer SHALL render the first Body in the sorted list centered at the world origin with the camera positioned on the positive Z axis looking toward the origin along the negative Z direction with Y-up orientation.
5. WHEN the left arrow key is pressed, THE Body_Viewer SHALL switch to displaying the previous Body in the loaded list (wrapping from first to last).
6. WHEN the right arrow key is pressed, THE Body_Viewer SHALL switch to displaying the next Body in the loaded list (wrapping from last to first).
7. WHILE the W key is held, THE Body_Viewer SHALL rotate the currently displayed Body upward (positive pitch) at a rate of 90 degrees per second scaled by frame delta time.
8. WHILE the S key is held, THE Body_Viewer SHALL rotate the currently displayed Body downward (negative pitch) at a rate of 90 degrees per second scaled by frame delta time.
9. WHILE the A key is held, THE Body_Viewer SHALL rotate the currently displayed Body leftward (negative yaw) at a rate of 90 degrees per second scaled by frame delta time.
10. WHILE the D key is held, THE Body_Viewer SHALL rotate the currently displayed Body rightward (positive yaw) at a rate of 90 degrees per second scaled by frame delta time.
11. IF the scanned directory contains no valid Body_JSON files, THEN THE Body_Viewer SHALL display an empty viewport with a black background (RGB 0.0, 0.0, 0.0) and log a message via SDL_Log indicating that no models were found.
12. IF a Body_JSON file in the directory fails to parse, THEN THE Body_Viewer SHALL skip that file, log a warning via SDL_Log including the filename and the parse error reason, and continue loading remaining files.
13. IF the specified directory path does not exist or cannot be read, THEN THE Body_Viewer SHALL log an error via SDL_Log indicating the inaccessible path and exit with a non-zero code.

### Requirement 10: SDL3 Window and Context Management

**User Story:** As a developer, I want the Body_Viewer to use SDL3 for window creation and OpenGL context management, so that the application integrates with the project's existing SDL3 dependency.

#### Acceptance Criteria

1. THE Body_Viewer SHALL create an SDL_Window with the SDL_WINDOW_OPENGL flag and a default size of 800x600 pixels with the title "Particluar Body Viewer".
2. THE Body_Viewer SHALL create an OpenGL context via SDL_GL_CreateContext associated with the created window.
3. THE Body_Viewer SHALL set SDL_GL_CONTEXT_MAJOR_VERSION to 2 and SDL_GL_CONTEXT_MINOR_VERSION to 1 before creating the context, requesting an OpenGL 2.1 compatibility profile.
4. THE Body_Viewer SHALL set SDL_GL_DOUBLEBUFFER to 1 and SDL_GL_DEPTH_SIZE to 24 as GL attribute hints before context creation.
5. THE Body_Viewer SHALL run an event loop that polls for SDL events each iteration and SHALL call SDL_GL_SwapWindow at the end of each frame.
6. IF SDL_Init with SDL_INIT_VIDEO fails, THEN THE Body_Viewer SHALL log the SDL error via SDL_Log and exit with a non-zero code.
7. IF window creation or OpenGL context creation fails, THEN THE Body_Viewer SHALL log the error via SDL_Log, destroy any successfully created resources (window, context) in reverse creation order, call SDL_Quit, and exit with a non-zero code.
8. WHEN the Body_Viewer receives an SDL_EVENT_QUIT event, THE Body_Viewer SHALL exit the event loop, destroy the OpenGL context via SDL_GL_DestroyContext, destroy the window via SDL_DestroyWindow, and call SDL_Quit before terminating with exit code 0.
