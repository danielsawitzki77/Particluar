# Spec: Body Composition Constraints and Subdivision Soundness

## Problem Statement

The body composition system currently allows any shape to connect to any other shape via face connections. However, when all models in a body share the same subdivision resolution (which can change at runtime), connections must only occur between faces that have the same topology in their subdivided state.

Currently:
- A **Box** has 6 quad faces
- A **Frustum** has N quad lateral faces + 2 N-gon caps
- A **Sphere** has triangular faces (poles) and quad faces (mid-bands)
- A **Cylinder** has N quad lateral faces + 2 N-gon caps
- A **Cone** has N triangular lateral faces + 1 N-gon cap
- A **Torus** has N×M quad faces

The core issue: a Box's quad face is inherently different from an N-gon cap face. When subdivision is applied uniformly, boxes and frustums don't scale consistently with radially-symmetric primitives. A cylinder's N-gon cap can match a sphere's N-gon approximation face, but a box face cannot.

## Solution

Remove **Box** and **Frustum** as primitives entirely. Every remaining primitive (Sphere, Cylinder, Cone, Torus) has faces that are generated from the same N-segment radial subdivision. This means:
- Cylinder cap → N-gon (matches sphere N-gon at same segment count)
- Cone base → N-gon (matches cylinder cap at same segment count)
- Sphere faces → triangles at poles, quads in bands (but connection faces are the N-gon approximations at pole caps)
- Torus → quads from ring × tube segments

With boxes and frustums removed, all remaining shapes share compatible face topologies at their connection points (N-gon caps and quad lateral faces derived from the same segment count).

## Requirements

### R1: Remove Box and Frustum Shape Types
- Remove `ShapeType::Box` and `ShapeType::Frustum` from the enum
- Remove `GenerateBox()` and `GenerateFrustum()` from `FaceGenerator`
- Remove parsing/serialization for "box" and "frustum" types in `BodyLoader`
- Remove Box and Frustum shape parameter fields that are exclusive to those types

### R2: Add Connection Compatibility Validation
- Add a validation step in `ConnectionSolver` or `BodyLoader` that checks face compatibility
- Two faces are compatible for connection if they have the same vertex count (same polygon topology)
- This ensures that when subdivision resolution changes, both faces remain congruent
- Validation should produce a clear error message identifying which connection is invalid

### R3: Update Body Definition Files
- Remove or convert `01_unit_cube.json` (uses box) → replace with a cylinder or sphere approximation
- Remove or convert `08_table.json` (uses box for tabletop) → replace with cylinder (flat disc)
- Update `06_rocket.json` (uses frustum for engine bell) → replace frustum with cone
- Verify all other body files don't reference box or frustum types

### R4: Update Tests
- Remove Box and Frustum test cases from `test_face_generator.cpp`
- Remove Box/Frustum shape generation in `test_body_loader.cpp`
- Add new test: verify connection compatibility validation rejects mismatched face counts
- Add new test: verify all remaining shape types can connect via compatible N-gon faces

### R5: Enforce `segments` Consistency at Connection Time
- When two shapes connect via Face_Connection, the connected faces must have the same vertex count
- The `segments` parameter determines face vertex count for caps (N-gon with N = segments)
- Validation: `parent_face.vertices.size() == child_face.vertices.size()` for Face_Connection type

## Design

### Shape Type Enum (After)

```cpp
enum class ShapeType {
    Cone,
    Cylinder,
    Sphere,
    Torus
};
```

### ShapeParams (After)

```cpp
struct ShapeParams {
    ShapeType type;

    // Cone/Cylinder: radius, height, segments
    float radius;
    float height;
    int segments;

    // Sphere: radius, lat_segments, lon_segments
    int lat_segments, lon_segments;

    // Torus: major_radius, minor_radius, ring_segments, side_segments
    float major_radius, minor_radius;
    int ring_segments, side_segments;

    ShapeParams()
        : type(ShapeType::Cylinder)
        , radius(0.5f), height(1.0f), segments(16)
        , lat_segments(12), lon_segments(16)
        , major_radius(1.0f), minor_radius(0.25f)
        , ring_segments(16), side_segments(8)
    {}
};
```

Removed fields: `width`, `depth` (box-only), `top_radius`, `bottom_radius` (frustum-only).

### Connection Compatibility Check

```cpp
// In ConnectionSolver or as a standalone validator
struct ValidationResult {
    bool valid;
    std::string error;
};

ValidationResult ValidateConnection(
    const Connection& conn,
    const std::vector<Face>& parent_faces,
    const std::vector<Face>& child_faces);
```

Returns invalid if:
- `conn.parent_face_index` is out of range
- `conn.child_face_index` is out of range
- For Face_Connection: `parent_faces[idx].vertices.size() != child_faces[idx].vertices.size()`

### Migration of Existing Body Files

| File | Current | Replacement |
|------|---------|-------------|
| `01_unit_cube.json` | Box 1×1×1 | Cylinder r=0.5 h=1.0 segments=4 (approximates a square cross-section) |
| `08_table.json` | Box tabletop | Cylinder r=1.0 h=0.1 segments=32 (flat disc) |
| `06_rocket.json` | Frustum engine bell | Cone r=0.6 h=0.5 sides=20 (tapered nozzle) |

## Tasks

1. [x] Write this spec
2. [ ] Remove ShapeType::Box and ShapeType::Frustum from BodyTypes.h
3. [ ] Remove GenerateBox() and GenerateFrustum() from FaceGenerator.h/.cpp
4. [ ] Remove box/frustum parsing and serialization from BodyLoader.cpp
5. [ ] Clean up ShapeParams (remove box/frustum-only fields)
6. [ ] Add ConnectionValidator class with face topology check
7. [ ] Integrate validation into BodyLoader (validate on load)
8. [ ] Update body JSON files (01, 06, 08)
9. [ ] Update tests (remove box/frustum cases, add compatibility tests)
10. [ ] Build and verify all tests pass
