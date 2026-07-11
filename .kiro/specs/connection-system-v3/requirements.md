# Requirements Document

## Introduction

This document specifies the requirements for Connection System V3 — a complete rewrite of the body renderer's connection system for the Particluar project. The new system positions connected bodies as independent closed meshes with coincident connection faces, replacing the broken v2 approach that attempted runtime topology changes (tapering, hole-cutting, face scaling) resulting in visual artifacts.

Phase 1 (rendering unmodified closed solids with correct positioning) is already complete and committed on branch `connection-system-v3`. This document covers Phases 2–4: face identification by grid position, size-matching deformation, and visual validation.

## Glossary

- **Body_Renderer**: The static library (`body_renderer/`) that generates, connects, and renders 3D body meshes from JSON definitions
- **ConnectionSolver**: The component that computes local transforms positioning child nodes so their connection faces are coincident with parent connection faces
- **ConnectionFaceMatcher**: The component responsible for face identification and size-matching deformation at connection points
- **FaceGenerator**: The component that generates closed mesh faces for each primitive shape type (Cylinder, Cone, Sphere, Torus, Capsule)
- **Face_Grid**: A logical 2D grid of faces produced by a subdivided primitive, indexed by (column, row) or (ring, tube) depending on shape type
- **Connection_Face**: The specific face on a shape identified at a connection point by mapping UV parameters to a grid index
- **Midpoint_Radius**: The average of parent and child connection radii used to scale connection faces to a matching size: `(parent_radius + child_radius) / 2`
- **N-gon**: A polygon with N vertices, specifically referring to cap faces on cylinders and cones
- **Quad**: A four-vertex polygon face, specifically the lateral faces on cylinders, torus surfaces, and sphere mid-bands
- **Face_Grid_Index**: A deterministic integer index into a shape's face array, computed from UV parameters and the shape's segment counts
- **Attachment_Point**: A parametric specification of where on a shape a connection occurs, consisting of region, u, and v parameters
- **Topological_Compatibility**: The property that two connection faces have the same polygon type (quad↔quad, ngon↔ngon, tri↔tri) at any subdivision level

## Requirements

### Requirement 1: Mesh Independence

**User Story:** As a developer, I want connected bodies to remain independent closed meshes, so that no topology changes are required and rendering remains artifact-free.

#### Acceptance Criteria

1. THE Body_Renderer SHALL render each body node as a complete closed solid mesh with all its original faces intact
2. WHEN two bodies are connected, THE ConnectionSolver SHALL position the child so its connection face is coincident with the parent's connection face without modifying face topology on either mesh
3. THE ConnectionFaceMatcher SHALL preserve the vertex count of every face during size-matching deformation (faces are never added, removed, or re-topologized)

### Requirement 2: Face Identification by Grid Position

**User Story:** As a developer, I want to identify connection faces by mapping UV parameters to face grid indices, so that face lookup is deterministic and independent of world-space position.

#### Acceptance Criteria

1. WHEN a Cylinder with segments=N and height_segments=H is queried, THE ConnectionFaceMatcher SHALL map UV parameters to a Face_Grid_Index using the formula: top cap = index 0, bottom cap = index 1, lateral quads = index 2 + row*N + col where col = floor(u * N) and row = floor(v * H)
2. WHEN a Cone with segments=N is queried, THE ConnectionFaceMatcher SHALL map UV parameters to a Face_Grid_Index using the formula: base cap = index 0, lateral triangles = index 1 + floor(u * N)
3. WHEN a Torus with ring_segments=R and tube_segments=T is queried, THE ConnectionFaceMatcher SHALL map UV parameters to a Face_Grid_Index using the formula: index = floor(u * R) * T + floor(v * T)
4. WHEN a Sphere with slices=S and stacks=T is queried, THE ConnectionFaceMatcher SHALL map UV parameters to a Face_Grid_Index using the formula: north pole triangles = indices 0..S-1, mid-band quads = index S + row*S + col, south pole triangles at the end
5. THE ConnectionFaceMatcher SHALL use grid-based face identification for all connection point lookups instead of world-space proximity searches

### Requirement 3: Topological Compatibility Validation

**User Story:** As a developer, I want the system to reject incompatible connections at load time, so that only faces of the same polygon type are connected and rendering correctness is guaranteed.

#### Acceptance Criteria

1. WHEN a body JSON file is loaded, THE Body_Renderer SHALL validate that each connection's parent and child attachment points produce faces of the same polygon type
2. WHEN a quad-producing attachment (cylinder side, torus surface, sphere mid-band) is connected to an ngon-producing attachment (cylinder cap, cone base), THEN THE Body_Renderer SHALL reject the connection and report an error
3. WHEN a triangle-producing attachment (cone side) is connected to a quad or ngon attachment, THEN THE Body_Renderer SHALL reject the connection and report an error
4. THE Body_Renderer SHALL permit connections between: quad↔quad (cylinder side ↔ torus surface ↔ sphere mid-band ↔ cylinder side), ngon↔ngon (cylinder top ↔ cylinder bottom ↔ cone base), and tri↔tri (cone side ↔ cone side)

### Requirement 4: Size-Matching Deformation

**User Story:** As a developer, I want connected shapes with different radii to meet at a midpoint size with smooth local stretching, so that connections look visually seamless without topology changes.

#### Acceptance Criteria

1. WHEN parent and child shapes have different radii at the connection point, THE ConnectionFaceMatcher SHALL compute the Midpoint_Radius as (parent_connection_radius + child_connection_radius) / 2
2. THE ConnectionFaceMatcher SHALL scale the Connection_Face vertices uniformly from the face's own center to reach the Midpoint_Radius
3. WHEN a Connection_Face is scaled, THE ConnectionFaceMatcher SHALL propagate shared vertex positions to neighboring faces that share edges with the Connection_Face, leaving non-shared vertices unchanged
4. THE ConnectionFaceMatcher SHALL apply size-matching deformation independently on each shape (parent and child) without requiring coordination between the two meshes
5. WHEN the Midpoint_Radius equals the shape's natural connection radius, THE ConnectionFaceMatcher SHALL leave the Connection_Face vertices unmodified

### Requirement 5: Height Segments for Lateral Quad Matching

**User Story:** As a developer, I want cylinder and cone height_segments to subdivide lateral faces into smaller quads, so that side-to-side connections with torus tube quads produce matching face sizes.

#### Acceptance Criteria

1. WHEN a Cylinder has height_segments=H (where H > 1), THE FaceGenerator SHALL produce N×H lateral quad faces organized in a grid of H rows and N columns
2. WHEN a Cylinder side face connects to a Torus tube face, THE ConnectionFaceMatcher SHALL verify that the cylinder's height_segments produces quads of compatible size to the torus tube_segments quads
3. THE FaceGenerator SHALL generate lateral quads with vertices at evenly spaced height intervals, with row 0 at the bottom and row H-1 at the top

### Requirement 6: Connection Positioning

**User Story:** As a developer, I want the ConnectionSolver to position children so connection faces are exactly coincident, so that the visual result shows two bodies touching perfectly at one face.

#### Acceptance Criteria

1. WHEN a parametric connection is resolved, THE ConnectionSolver SHALL orient the child so its connection face normal points opposite to the parent's connection face normal
2. WHEN a parametric connection is resolved, THE ConnectionSolver SHALL translate the child so its connection face center coincides with the parent's connection face center
3. WHEN a rotation value is specified in the connection, THE ConnectionSolver SHALL apply a spin rotation around the parent's connection face normal by the specified angle in degrees

### Requirement 7: Visual Validation of Example Models

**User Story:** As a developer, I want all example body models to render correctly with the new connection system, so that the rewrite is confirmed working for all supported configurations.

#### Acceptance Criteria

1. THE Body_Renderer SHALL render all 12 example models in assets/bodies/ without visual artifacts (inverted faces, gaps, or skewed geometry)
2. WHEN an example model contains an incompatible connection, THE Body_Renderer SHALL report the incompatibility at load time and either skip the invalid connection or reject the model
3. THE Body_Renderer SHALL produce visually identical positioning results to Phase 1 (unmodified meshes touching at connection faces) for all models that do not require size matching
