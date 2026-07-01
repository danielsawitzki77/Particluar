# Requirements Document

## Introduction

An independent OpenGL-based 3D body renderer module for the Particluar project. The module renders flat-shaded, topologically closed 3D bodies composed of connected geometric primitives (boxes, cones, cylinders, spheres, tori, frustums), defined via a JSON-based model format. The renderer uses legacy OpenGL 2 with Phong lighting, routed through SDL3 for window and OpenGL context management. A proof-of-concept viewer application demonstrates the renderer by loading and displaying models from a directory with keyboard-driven navigation and rotation.

## Glossary

- **Body_Renderer**: The static library (.lib) within Particluar.sln responsible for OpenGL 2 rendering of flat-shaded 3D bodies with Phong lighting.
- **Body**: A complete 3D model defined as an acyclic tree of connected geometric primitives, loaded from a Body_JSON file.
- **Body_JSON**: A JSON file describing a Body as a shape tree of primitives with connection parameters.
- **Shape_Tree**: An acyclic tree data structure where each node is a geometric primitive connected to its parent at a specified attachment point.
- **Primitive**: A basic geometric solid used as a building block in a Body. Supported types: Box, Cone, Cylinder, Sphere, Torus, Frustum.
- **Box**: A rectangular parallelepiped primitive with configurable width, height, and depth dimensions. Has 6 named flat faces.
- **Cone**: A conical primitive with configurable base radius, height, and lateral resolution. Has a flat circular base face, a point tip, and a lateral (side) surface.
- **Cylinder**: A cylindrical primitive with configurable radius, height, and lateral resolution. Has flat circular top and bottom cap faces and a lateral (side) surface.
- **Sphere**: A spherical primitive with configurable radius, latitudinal resolution, and longitudinal resolution. Has no flat faces; connections occur at pole points or along ring edges.
- **Torus**: A toroidal primitive with configurable major radius, minor radius, and ring/tube resolution. Has no flat faces; connections occur along ring edges on the outer or inner equator.
- **Frustum**: A truncated cone primitive with configurable top radius, bottom radius, height, and lateral resolution. Has flat circular top and bottom cap faces and a lateral (side) surface.
- **Connection**: The attachment relationship between a child primitive and its parent in the Shape_Tree, defined by the parent's attachment point, the child's attachment point, and a rotation_position parameter.
- **Attachment_Point**: A named location on a primitive where another primitive may connect. Can be a flat face (planar polygon), a point (zero-area tip), or a ring edge (circle where two surfaces meet).
- **Edge_Connection**: A connection where two primitives share a ring edge (a circle/polygon approximation). The shared edge becomes a line of coincident vertices in both meshes.
- **Face_Connection**: A connection where a child primitive's flat face is placed coincident with and flush against a parent primitive's flat face. The two faces merge into the interior of the composite body and are not rendered.
- **Point_Connection**: A connection where a child primitive attaches at a point (e.g., the tip of a cone or a pole of a sphere). The child's attachment point is coincident with the parent's point.
- **Rotation_Position**: A floating-point parameter in the range 0.0 to 1.0 that defines the rotational orientation of a child primitive relative to its parent's connection normal/axis. 0.0 and 1.0 represent the same orientation (full rotation wraps).
- **Face**: A flat polygon resulting from decomposing a primitive's surface. Faces can be triangles, quads, or N-gons depending on the primitive type.
- **Face_Decomposition**: The process of converting a Body's primitives into a list of planar polygon faces with computed normals.
- **Triangle_Decomposition**: The process of subdividing polygon faces into triangles suitable for OpenGL rendering.
- **Phong_Lighting**: The illumination model combining ambient, diffuse, and specular components computed per-face (flat shading).
- **Point_Light**: A light source defined by a 3D position and color (RGB), contributing to the Phong_Lighting calculation.
- **Flat_Shading**: A rendering technique where each triangle face receives a single uniform color computed from the face normal, with no interpolation across vertices.
- **Body_Viewer**: The proof-of-concept executable that loads Body_JSON files from a directory and renders them with interactive rotation and model switching.
- **Lateral_Resolution**: The integer number of sides used to approximate curved surfaces as polygon meshes. Higher values produce smoother approximations.
- **Non-Intersecting Constraint**: The fundamental rule that connected primitives in a Body SHALL NOT have their solid volumes overlap. Connections are surface-to-surface, edge-to-edge, or point-to-point only.
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
### Requirement 4: Supported Primitive Types

**User Story:** As a content creator, I want a rich set of geometric primitives to compose bodies from, so that I can model a wide variety of shapes without resorting to manual mesh editing.

#### Acceptance Criteria

1. THE Body_Renderer SHALL support the following primitive types: Box, Cone, Cylinder, Sphere, Torus, and Frustum.
2. THE Body_JSON SHALL define Box dimensions as: "width" (float, 0.001 to 1000.0), "height" (float, 0.001 to 1000.0), "depth" (float, 0.001 to 1000.0).
3. THE Body_JSON SHALL define Cone dimensions as: "radius" (float, 0.001 to 1000.0), "height" (float, 0.001 to 1000.0), "sides" (integer, 3 to 128).
4. THE Body_JSON SHALL define Cylinder dimensions as: "radius" (float, 0.001 to 1000.0), "height" (float, 0.001 to 1000.0), "sides" (integer, 3 to 128).
5. THE Body_JSON SHALL define Sphere dimensions as: "radius" (float, 0.001 to 1000.0), "slices" (integer, 4 to 128, longitudinal divisions), "stacks" (integer, 3 to 64, latitudinal divisions).
6. THE Body_JSON SHALL define Torus dimensions as: "major_radius" (float, 0.001 to 1000.0), "minor_radius" (float, 0.001 to 1000.0, must be less than major_radius), "ring_segments" (integer, 3 to 128), "tube_segments" (integer, 3 to 64).
7. THE Body_JSON SHALL define Frustum dimensions as: "top_radius" (float, 0.0 to 1000.0), "bottom_radius" (float, 0.001 to 1000.0, must be greater than top_radius), "height" (float, 0.001 to 1000.0), "sides" (integer, 3 to 128). WHEN top_radius is 0.0, THE Frustum degenerates to a Cone.
8. EACH primitive type SHALL expose named attachment points as defined in Requirement 8 (Connection System).

### Requirement 5: Body JSON Data Format

**User Story:** As a content creator, I want a JSON-based format to define 3D bodies as trees of connected primitives, so that models are human-readable, easy to author, and tooling-friendly.

#### Acceptance Criteria

1. THE Body_JSON SHALL be a JSON object with a root-level "name" field (string, 1 to 128 characters) and a "root" field containing the root primitive node of the Shape_Tree.
2. THE Body_JSON SHALL define each primitive node as a JSON object containing: "type" (string: "box", "cone", "cylinder", "sphere", "torus", or "frustum"), "dimensions" (object with type-specific size parameters), "color" (object with "r", "g", "b" fields, each 0.0 to 1.0), and an optional "children" array of child connection objects. IF the "children" field is absent or is an empty array, THEN THE Body_JSON Parser SHALL treat the node as a leaf node with zero children.
3. THE Body_JSON SHALL define each child connection object as containing: "parent_attachment" (string identifying the attachment point on the parent), "child_attachment" (string identifying the attachment point on the child), "rotation_position" (float, 0.0 to 1.0), and "node" (the child primitive node object).
4. THE Shape_Tree encoded in Body_JSON SHALL be acyclic and SHALL have a maximum nesting depth of 32 levels; IF a node references itself, creates a cycle through its children, or exceeds the maximum depth, THEN THE Body_JSON Parser SHALL reject the file and report an error indicating the structural violation.
5. IF a primitive node is missing any required field ("type", "dimensions", or "color"), THEN THE Body_JSON Parser SHALL reject the file and report an error identifying the missing field and its location in the tree.
6. THE Body_JSON SHALL support an optional root-level "material" object with "shininess" (float, 1.0 to 128.0, default 32.0) and "ambient" (object with "r", "g", "b", default 0.1 each) to configure the global material properties of the body.

### Requirement 6: Body JSON Parsing and Validation

#### Acceptance Criteria

1. WHEN a valid Body_JSON file is provided, THE Body_JSON Parser SHALL produce an in-memory Shape_Tree representation containing all primitive nodes with their types, dimensions, colors, and connection parameters.
2. THE Body_JSON Pretty_Printer SHALL format a Shape_Tree back into a valid Body_JSON file with 2-space indentation as produced by picojson's serialize(true), terminated by a single newline character.
3. FOR ALL valid Body objects, parsing the Body_JSON, pretty-printing the result, and parsing again SHALL produce a Shape_Tree where primitive types, connection identifiers, rotation_position values, and tree structure are identical, and where all floating-point values are equal within a tolerance of 1e-9 (round-trip property).
4. IF a Body_JSON file contains malformed JSON, THEN THE Body_JSON Parser SHALL reject the input and report an error message indicating the line number and surrounding text where parsing failed.
5. IF a Body_JSON file contains a primitive with an unrecognized "type" value, THEN THE Body_JSON Parser SHALL reject the file and report an error identifying the invalid type string and the node's path from the root.
6. IF a Body_JSON file contains a dimension value that is not a positive finite number (or zero where explicitly allowed for Frustum top_radius), or a resolution value outside its allowed range, THEN THE Body_JSON Parser SHALL reject the file and report an error identifying the invalid field.
7. IF a Body_JSON file contains a color component outside the range 0.0 to 1.0, THEN THE Body_JSON Parser SHALL clamp the value to the nearest bound (0.0 or 1.0) and log a warning identifying the field name and the original value.
8. IF a Body_JSON specifies a connection between two attachment points whose types are incompatible according to the Connection Compatibility Matrix (Requirement 8), THEN THE Body_JSON Parser SHALL reject the file and report an error identifying the incompatible connection.

### Requirement 7: Primitive Face Decomposition

**User Story:** As a developer, I want each primitive type to decompose into polygon faces with computed normals, so that the geometry pipeline can convert any Body into renderable triangles.

#### Acceptance Criteria

1. WHEN a Box primitive is decomposed, THE Face_Decomposition SHALL produce exactly 6 quadrilateral faces (one per face of the rectangular parallelepiped), each with an outward-pointing normal and vertices in counter-clockwise winding order as seen from outside the solid.
2. WHEN a Cone primitive with N sides is decomposed, THE Face_Decomposition SHALL produce N triangular lateral faces and 1 N-gon base face, each with an outward-pointing normal and counter-clockwise winding.
3. WHEN a Cylinder primitive with N sides is decomposed, THE Face_Decomposition SHALL produce N quadrilateral lateral faces, 1 N-gon top cap face, and 1 N-gon bottom cap face, each with an outward-pointing normal and counter-clockwise winding.
4. WHEN a Sphere primitive with S slices and T stacks is decomposed, THE Face_Decomposition SHALL produce S triangular faces at each pole and S*(T-2) quadrilateral faces for intermediate latitude bands, each with an outward-pointing normal.
5. WHEN a Frustum primitive with N sides is decomposed, THE Face_Decomposition SHALL produce N quadrilateral lateral faces (or N triangular faces if top_radius is 0.0), plus 1 N-gon top cap face (if top_radius > 0) and 1 N-gon bottom cap face, each with an outward-pointing normal.
6. THE Face_Decomposition SHALL compute each face normal as the cross product of two consecutive edge vectors of that face, oriented outward from the primitive's solid interior.
7. WHEN a child primitive is decomposed, THE Face_Decomposition SHALL first apply the Connection's translation (derived from parent_attachment and child_attachment center positions) and then apply the rotation (derived from rotation_position) to position the child relative to its parent, before computing the child's face vertices and normals.
8. WHEN a Face_Connection joins two primitives, THE Face_Decomposition SHALL suppress (not emit) the two coincident faces at the connection boundary, since they become interior to the composite solid.
9. WHEN an Edge_Connection joins two primitives, THE Face_Decomposition SHALL merge the shared ring of vertices so that adjacent faces from both primitives reference the same vertex positions at the shared boundary, ensuring a watertight mesh without duplicate edges.
10. THE Face_Decomposition SHALL produce a topologically closed mesh for the entire composite Body, meaning every edge in the final mesh is shared by exactly 2 faces.

**User Story:** As a developer, I want polygon faces to be subdivided into triangles for OpenGL rendering, so that all geometry is expressed as triangles regardless of the original face shape.

#### Acceptance Criteria

1. WHEN a triangular face (3 vertices) is provided, THE Triangle_Decomposition SHALL output that face unchanged as a single triangle.
2. WHEN a quadrilateral face (4 vertices) is provided, THE Triangle_Decomposition SHALL subdivide it into exactly 2 triangles sharing the original face normal.
3. WHEN an N-gon face with N greater than 4 is provided, THE Triangle_Decomposition SHALL subdivide it into exactly (N - 2) triangles using fan triangulation from the first vertex, each sharing the original face normal.
4. THE Triangle_Decomposition SHALL preserve the winding order of the original face vertices such that all output triangles have consistent front-face orientation (counter-clockwise when viewed from outside the solid).
5. THE Triangle_Decomposition SHALL produce triangles whose combined area equals the area of the original convex polygon face (no gaps, no overlaps).
6. IF a face with fewer than 3 vertices is provided, THEN THE Triangle_Decomposition SHALL produce zero triangles and discard the degenerate face.

### Requirement 10: Body Viewer Application

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
