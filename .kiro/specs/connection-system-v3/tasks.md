# Implementation Plan: Connection System V3 — Phases 2 & 3

## Overview

This plan implements grid-based face identification (Phase 2) and size-matching deformation (Phase 3) for the Connection System V3 rewrite. Phase 1 (unmodified closed solids with correct positioning) is already complete on branch `connection-system-v3`.

The implementation replaces the O(n) proximity-based face lookup with O(1) algebraic grid-index computation, adds topological compatibility validation at load time, and implements uniform-scale deformation to make connection faces meet at a midpoint radius.

## Tasks

- [ ] 1. Implement grid-based face identification (Phase 2)
  - [ ] 1.1 Implement `ComputeGridIndex` for Cylinder and Cone
    - Add `int ComputeGridIndex(const ShapeParams& shape, const AttachmentPoint& attach) const` to `ConnectionFaceMatcher`
    - Cylinder: top cap = `N*H`, bottom cap = `N*H + 1`, lateral = `row * N + col` where `col = clamp(floor(u * N), 0, N-1)`, `row = clamp(floor(v * H), 0, H-1)`
    - Cone: base cap = `N`, lateral triangles = `clamp(floor(u * N), 0, N-1)`
    - Clamp UV inputs to [0, 1) before computation
    - Clamp output to valid range [0, face_count-1]
    - _Requirements: 2.1, 2.2, 2.5_

  - [ ] 1.2 Implement `ComputeGridIndex` for Torus and Sphere
    - Torus: `col = clamp(floor(u * R), 0, R-1)`, `row = clamp(floor(v * T), 0, T-1)`, index = `col * T + row`
    - Sphere: compute `stack_f = v * T`, if `< 1.0` → north pole tri (index = col), if `>= T-1` → south pole tri, else → mid-band quad at `S + row*S + col`
    - Handle Capsule as a composite of pole triangles, hemisphere quads, and cylinder lateral quads with appropriate offset computation
    - _Requirements: 2.3, 2.4, 2.5_

  - [ ] 1.3 Replace proximity search with grid-index lookup in `GenerateWithConnections`
    - Remove the O(n) proximity loop from `GenerateWithConnections`
    - Use `ComputeGridIndex` to deterministically find the connection face index for each ring
    - Derive the `AttachmentPoint` from the `ConnectionRing` data (or pass it through from the connection definition)
    - Ensure `connection_face_indices` is populated from grid-index results
    - _Requirements: 2.5, 1.1_

  - [ ]* 1.4 Write property test: Grid Index Correctness (Property 1)
    - **Property 1: Grid Index Correctness**
    - For any valid shape with random segment params and random UV in [0, 1), `ComputeGridIndex` returns the index of the face whose geometric center is closest to the parametric surface point
    - Generate random ShapeParams (type, segments [3,32], radii [0.1,5.0], heights [0.2,10.0], height_segments [1,8])
    - Generate random AttachmentPoints with valid region for shape type, u and v in [0.0, 0.999]
    - Minimum 100 iterations with deterministic seed
    - **Validates: Requirements 2.1, 2.2, 2.3, 2.4, 2.5**

  - [ ]* 1.5 Write unit tests for `ComputeGridIndex` with known values
    - Cylinder cap at `region==Top` → index `N*H`
    - Torus at u=0, v=0 → index 0
    - Sphere pole triangles at v≈0 → index in [0, S-1]
    - Sphere pole triangles at v≈1 → south pole range
    - Cone base at `region==Base` → index N
    - _Requirements: 2.1, 2.2, 2.3, 2.4_

- [ ] 2. Implement topological compatibility validation (Phase 2)
  - [ ] 2.1 Implement `GetPolygonTypeAtAttachment` and `ValidateConnectionCompatibility`
    - Add `enum class FacePolygonType { Ngon, Quad, Triangle }` to header
    - Implement `GetPolygonTypeAtAttachment(const ShapeParams&, const AttachmentPoint&)` using the polygon type table from the design
    - Implement `ValidateConnectionCompatibility(const Connection&, const ShapeParams& parent, const ShapeParams& child)` — returns true iff both sides have the same polygon type
    - _Requirements: 3.1, 3.2, 3.3, 3.4_

  - [ ] 2.2 Integrate validation into body loading
    - Call `ValidateConnectionCompatibility` during body JSON loading for each connection
    - On failure: log error with connection index, parent/child attachment info, and mismatched polygon types
    - Skip the invalid connection (child positioned at identity transform) — do not crash
    - Continue loading the rest of the model
    - _Requirements: 3.1, 3.2, 3.3_

  - [ ]* 2.3 Write property test: Topological Compatibility (Property 11)
    - **Property 11: Topological Compatibility Validation**
    - For any connection definition, `ValidateConnectionCompatibility` returns true iff polygon type at parent attachment equals polygon type at child attachment
    - Generate random pairs of shape+attachment, verify acceptance/rejection matches the type table
    - Minimum 100 iterations
    - **Validates: Requirements 3.1, 3.2, 3.3, 3.4**

  - [ ]* 2.4 Write unit tests for compatibility validation
    - cylinder top ↔ cylinder bottom → accepted (ngon ↔ ngon)
    - cylinder side ↔ cone base → rejected (quad ↔ ngon)
    - cone side ↔ sphere pole → accepted (tri ↔ tri)
    - cylinder side ↔ torus surface → accepted (quad ↔ quad)
    - _Requirements: 3.2, 3.3, 3.4_

- [ ] 3. Checkpoint — Verify Phase 2
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 4. Implement size-matching deformation (Phase 3)
  - [ ] 4.1 Implement `DeformFaceToRadius`
    - Add `void DeformFaceToRadius(std::vector<Face>& faces, int face_index, float target_radius) const` to `ConnectionFaceMatcher`
    - Compute face center as average of vertices
    - Compute current_radius as average distance from center to each vertex
    - If `|current_radius - target_radius| < 1e-5f`: no-op
    - If `current_radius ≈ 0`: skip deformation (avoid division by zero)
    - Compute `scale_factor = target_radius / current_radius`
    - Scale each vertex: `V_new = face_center + (V - face_center) * scale_factor`
    - _Requirements: 4.2, 4.5_

  - [ ] 4.2 Implement shared vertex propagation
    - After scaling a connection face, find neighboring faces that share edge vertices (by position equality within epsilon 1e-5f)
    - Update shared vertex positions to match the new scaled positions
    - Leave non-shared vertices unchanged
    - At most 1 ring of adjacent faces is affected
    - _Requirements: 4.3_

  - [ ] 4.3 Wire deformation into `GenerateWithConnections`
    - After grid-index lookup identifies the connection face, compute the midpoint radius from the `ConnectionRing`'s target radius
    - If midpoint radius differs from the face's natural radius: call `DeformFaceToRadius`
    - Apply deformation independently on each shape (parent and child sides processed separately)
    - Remove the old `TaperCylinderEnd`, `TaperSphereRegion`, `TaperCapsuleRegion`, `TaperTorusRegion` method declarations from header (they are unused Phase 2 forward-declarations)
    - _Requirements: 4.1, 4.4_

  - [ ]* 4.4 Write property test: Face Count Preservation (Property 5)
    - **Property 5: Face Count Preservation**
    - For any shape and any set of connection rings, `GenerateWithConnections` produces the same number of faces as `FaceGenerator::Generate`
    - Minimum 100 iterations
    - **Validates: Requirements 1.1**

  - [ ]* 4.5 Write property test: Vertex Count Preservation (Property 6)
    - **Property 6: Vertex Count Preservation**
    - For any shape and deformation, every face in the output has the same vertex count as the corresponding face in unmodified output
    - Minimum 100 iterations
    - **Validates: Requirements 1.3**

  - [ ]* 4.6 Write property test: Deformation Achieves Target Radius (Property 7)
    - **Property 7: Deformation Achieves Target Radius**
    - After deformation, the average distance from connection face center to its vertices equals the target midpoint radius within epsilon 1e-4
    - Minimum 100 iterations
    - **Validates: Requirements 4.2**

  - [ ]* 4.7 Write property test: Shared Vertex Continuity (Property 8)
    - **Property 8: Shared Vertex Continuity**
    - After deformation, adjacent faces that shared an edge before still share that edge (coincident vertices within 1e-5)
    - Minimum 100 iterations
    - **Validates: Requirements 4.3**

  - [ ]* 4.8 Write property test: No-Op When Radii Match (Property 10)
    - **Property 10: No-Op When Radii Match**
    - When target radius equals natural connection radius, output vertices are identical to unmodified output within 1e-6
    - Minimum 100 iterations
    - **Validates: Requirements 4.5**

  - [ ]* 4.9 Write property test: Midpoint Radius Is Average (Property 9)
    - **Property 9: Midpoint Radius Is Average**
    - `ComputeMatchedRadius` returns `(parent_r + child_r) / 2` clamped to minimum 0.01
    - Minimum 100 iterations
    - **Validates: Requirements 4.1**

- [ ] 5. Checkpoint — Verify Phase 3
  - Ensure all tests pass, ask the user if questions arise.

- [ ] 6. Integration validation with example models
  - [ ] 6.1 Write integration test: load all 12 example body JSON files
    - Load each of the 12 body files in `assets/bodies/`
    - Verify no NaN/Inf in any vertex position or normal
    - Verify all normals are unit-length (within epsilon)
    - Verify face counts match expected totals for each shape type
    - Verify no topological compatibility errors for valid models
    - Report any incompatible connections at load time
    - _Requirements: 7.1, 7.2, 7.3_

  - [ ]* 6.2 Write property tests: Connection positioning (Properties 2, 3, 4)
    - **Property 2: Connection Face Centers Coincide** — after transform, child connection face center is within 1e-4 of parent's
    - **Property 3: Connection Normals Anti-Parallel** — dot product of normals within epsilon of -1.0
    - **Property 4: Spin Rotation Correctness** — child orientation differs from zero-rotation case by exactly θ degrees
    - Minimum 100 iterations each
    - **Validates: Requirements 1.2, 6.1, 6.2, 6.3**

- [ ] 7. Final checkpoint — Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

## Notes

- Tasks marked with `*` are optional and can be skipped for faster MVP
- Each task references specific requirements for traceability
- Checkpoints ensure incremental validation
- Property tests validate universal correctness properties from the design document
- Unit tests validate specific examples and edge cases
- All work targets branch `connection-system-v3` using C++14 / MSVC / Windows x64
- The existing test harness uses rapidcheck for property-based testing
- Phase 1 proximity search in `GenerateWithConnections` is replaced in task 1.3, not extended

## Task Dependency Graph

```json
{
  "waves": [
    { "id": 0, "tasks": ["1.1", "2.1"] },
    { "id": 1, "tasks": ["1.2", "2.2"] },
    { "id": 2, "tasks": ["1.3", "2.3", "2.4"] },
    { "id": 3, "tasks": ["1.4", "1.5"] },
    { "id": 4, "tasks": ["4.1"] },
    { "id": 5, "tasks": ["4.2"] },
    { "id": 6, "tasks": ["4.3"] },
    { "id": 7, "tasks": ["4.4", "4.5", "4.6", "4.7", "4.8", "4.9"] },
    { "id": 8, "tasks": ["6.1"] },
    { "id": 9, "tasks": ["6.2"] }
  ]
}
```
