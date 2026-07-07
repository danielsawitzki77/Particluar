# Spec: Parametric Connection System & Shape Improvements

## Problem Statement

The current body file format uses **face indices** (`parent_face`, `child_face`) to specify where shapes connect. But face breakdown into indexed faces is variable and runtime-dependent — changing the subdivision level renumbers all faces. This makes the file format fragile and semantically incorrect. The user should express connection intent in terms of the **geometric primitives themselves** (e.g., "attach to the top of this cylinder" or "attach at this point on the sphere's surface"), not in terms of a specific triangulation.

### Issues with current approach:
1. Face IDs in the file format are coupled to subdivision resolution
2. Users must know internal face numbering to author body files
3. Changing subdivision breaks connections
4. No capsule primitive exists

## Solution

Replace face-index-based connections with **parametric attachment coordinates** that describe WHERE on a shape's geometry a connection occurs, using normalized 0–1 parameters along the shape's natural dimensions. At runtime, subdivision generates faces, and the system finds/creates the appropriate N-gon face at the parametric location for the actual geometric connection.

## New Connection Model

### Attachment Point (replaces `parent_face`/`child_face`)

Each shape type defines its own parametric coordinate system:

| Shape | Regions | Parameters |
|-------|---------|------------|
| **Sphere** | `surface` | `theta` (0–1 = 0–360° longitude), `phi` (0–1 = 0–180° latitude, 0=top pole, 1=bottom pole) |
| **Cylinder** | `top`, `bottom`, `side` | For caps: `u` (0–1 radial), `v` (0–1 angular). For side: `u` (0–1 angular around), `v` (0–1 along height, 0=bottom, 1=top) |
| **Cone** | `base`, `side` | For base: `u` (0–1 radial), `v` (0–1 angular). For side: `u` (0–1 angular), `v` (0–1 height from base to tip) |
| **Torus** | `surface` | `u` (0–1 ring angle), `v` (0–1 tube angle) |
| **Capsule** | `top_cap`, `bottom_cap`, `side` | For caps: `theta` (0–1 angular), `phi` (0–1 from pole to equator). For side: `u` (0–1 angular), `v` (0–1 height) |

### Connection Definition (new JSON format)

```json
{
  "connection": {
    "parent_attach": { "region": "top", "u": 0.5, "v": 0.5 },
    "child_attach": { "region": "bottom", "u": 0.5, "v": 0.5 },
    "rotation": 0.0
  }
}
```

- `parent_attach`: where on the parent shape the child connects
- `child_attach`: which part of the child is used as the connection surface
- `rotation`: degrees of rotation around the connection normal

### Runtime Resolution

When subdivision occurs:

1. The parametric coordinate identifies a **point** on the shape's surface
2. The FaceGenerator creates an N-gon face centered at that point
3. Connected shapes **share the same N** for their connection faces (segment count matched)
4. If the natural face size differs between shapes (e.g., sphere face vs cylinder cap), the connection face size is matched by adjusting geometry locally at the joint

### Face Size Matching

When two shapes connect and their faces differ in size:
- The connection face polygon count (N) is determined by the shape with fewer segments at that region
- Both shapes generate matching N-gon faces at the connection point
- If necessary, one shape's geometry near the connection adapts to match the other's face size (radius transition)

## Requirements

### R1: New Connection Data Types
- Replace `Connection` struct's `parent_face_index`/`child_face_index` with `AttachmentPoint` structs
- `AttachmentPoint` has: `region` (enum per shape type), `u` (float 0–1), `v` (float 0–1)
- Keep `rotation` (degrees around connection normal)
- Keep `ConnectionType` as `Face_Connection` only (Edge and Point connections become parametric face connections with specific region selections)

### R2: New File Format (v2)
- Connection JSON changes from face indices to parametric regions
- Add `"format_version": 2` at root level for new files
- Loader supports both v1 (legacy face indices) and v2 (parametric) for migration
- Legacy v1 files: best-effort conversion of face indices to parametric regions at load time

### R3: Updated ConnectionSolver
- Accepts parametric `AttachmentPoint` instead of face indices
- Computes the 3D surface point from parametric coords
- Computes surface normal at that point
- Generates the transform to place child at parent's attachment point
- Coordinates with FaceGenerator to produce matching connection faces

### R4: Add Capsule Shape Type
- Two hemispheres connected by a cylinder
- Parameters: `radius`, `height` (total height including caps)
- Regions: `top_cap` (hemisphere), `bottom_cap` (hemisphere), `side` (cylinder portion)
- FaceGenerator produces: hemisphere triangle fans at caps, quad strips for the cylindrical middle

### R5: Per-Shape Scale Animation in Viewer
- New `ShapeScaleAnimator` class (separate from `JointAnimator`)
- Cycles through shapes in the body, animating their individual scale parameters:
  - Cylinder: radius and height (independently)
  - Sphere: radius
  - Cone: radius and height
  - Torus: major_radius and minor_radius
  - Capsule: radius and height
- Alternates scaling each parameter up and down (e.g., 0.5x to 1.5x range)
- Toggled independently from joint animation (new hotkey in viewer)

### R6: Update All Body Files to v2 Format
- Convert all `assets/bodies/*.json` to use parametric connections
- Remove all `parent_face`/`child_face` integer references
- Add `format_version: 2`

### R7: Update Tests
- Update `test_connection_solver.cpp` for new parametric API
- Update `test_connection_validator.cpp` for new validation logic
- Add tests for capsule face generation
- Add tests for parametric-to-face resolution
- Add tests for face size matching logic

## Design

### AttachmentPoint

```cpp
enum class AttachRegion {
    // Generic
    Surface,
    // Cylinder/Cone/Capsule specific
    Top,
    Bottom,
    Side,
    // Capsule specific
    TopCap,
    BottomCap
};

struct AttachmentPoint {
    AttachRegion region;
    float u;  // 0.0–1.0 first parametric coord
    float v;  // 0.0–1.0 second parametric coord

    AttachmentPoint() : region(AttachRegion::Top), u(0.5f), v(0.5f) {}
};
```

### Updated Connection

```cpp
struct Connection {
    AttachmentPoint parent_attach;
    AttachmentPoint child_attach;
    float rotation; // degrees around connection normal

    Connection() : rotation(0.0f) {}
};
```

### Parametric-to-3D Resolution

```cpp
struct SurfacePoint {
    Vec3 position;
    Vec3 normal;
};

class ParametricResolver {
public:
    // Given shape params and attachment point, compute the 3D surface position and normal
    SurfacePoint Resolve(const ShapeParams& shape, const AttachmentPoint& attach) const;

private:
    SurfacePoint ResolveSphere(const ShapeParams& s, const AttachmentPoint& a) const;
    SurfacePoint ResolveCylinder(const ShapeParams& s, const AttachmentPoint& a) const;
    SurfacePoint ResolveCone(const ShapeParams& s, const AttachmentPoint& a) const;
    SurfacePoint ResolveTorus(const ShapeParams& s, const AttachmentPoint& a) const;
    SurfacePoint ResolveCapsule(const ShapeParams& s, const AttachmentPoint& a) const;
};
```

### ShapeParams with Capsule

```cpp
enum class ShapeType {
    Cone,
    Cylinder,
    Sphere,
    Torus,
    Capsule
};

struct ShapeParams {
    ShapeType type;

    // Shared
    float radius;
    float height;
    int segments;

    // Sphere
    int lat_segments, lon_segments;

    // Torus
    float major_radius, minor_radius;
    int ring_segments, side_segments;

    // (Capsule uses radius, height, segments — same fields as cylinder)
};
```

### JSON v2 Example — Snowman

```json
{
  "format_version": 2,
  "name": "Snowman",
  "root": {
    "name": "body",
    "shape": { "type": "sphere", "radius": 1.0, "slices": 16, "stacks": 12 },
    "color": { "r": 0.95, "g": 0.95, "b": 0.98 },
    "children": [
      {
        "connection": {
          "parent_attach": { "region": "surface", "u": 0.5, "v": 0.0 },
          "child_attach": { "region": "surface", "u": 0.5, "v": 1.0 },
          "rotation": 0.0
        },
        "node": {
          "name": "torso",
          "shape": { "type": "sphere", "radius": 0.7, "slices": 16, "stacks": 12 },
          "color": { "r": 0.93, "g": 0.93, "b": 0.96 },
          "children": [
            {
              "connection": {
                "parent_attach": { "region": "surface", "u": 0.5, "v": 0.0 },
                "child_attach": { "region": "surface", "u": 0.5, "v": 1.0 },
                "rotation": 0.0
              },
              "node": {
                "name": "head",
                "shape": { "type": "sphere", "radius": 0.5, "slices": 16, "stacks": 12 },
                "color": { "r": 0.9, "g": 0.9, "b": 0.95 }
              }
            }
          ]
        }
      }
    ]
  }
}
```

### JSON v2 Example — Robot Arm

```json
{
  "format_version": 2,
  "name": "Robot Arm",
  "root": {
    "name": "base_cylinder",
    "shape": { "type": "cylinder", "radius": 0.4, "height": 2.0, "sides": 16 },
    "color": { "r": 0.5, "g": 0.5, "b": 0.55 },
    "children": [
      {
        "connection": {
          "parent_attach": { "region": "top", "u": 0.5, "v": 0.5 },
          "child_attach": { "region": "surface", "u": 0.5, "v": 1.0 },
          "rotation": 0.0
        },
        "node": {
          "name": "joint",
          "shape": { "type": "sphere", "radius": 0.5, "slices": 12, "stacks": 8 },
          "color": { "r": 0.7, "g": 0.3, "b": 0.2 },
          "children": [
            {
              "connection": {
                "parent_attach": { "region": "surface", "u": 0.5, "v": 0.0 },
                "child_attach": { "region": "bottom", "u": 0.5, "v": 0.5 },
                "rotation": 45.0
              },
              "node": {
                "name": "forearm",
                "shape": { "type": "cylinder", "radius": 0.3, "height": 1.5, "sides": 16 },
                "color": { "r": 0.5, "g": 0.5, "b": 0.55 },
                "children": [
                  {
                    "connection": {
                      "parent_attach": { "region": "top", "u": 0.5, "v": 0.5 },
                      "child_attach": { "region": "bottom", "u": 0.5, "v": 0.5 },
                      "rotation": 0.0
                    },
                    "node": {
                      "name": "hand",
                      "shape": { "type": "cone", "radius": 0.35, "height": 0.8, "sides": 16 },
                      "color": { "r": 0.9, "g": 0.7, "b": 0.1 }
                    }
                  }
                ]
              }
            }
          ]
        }
      }
    ]
  }
}
```

### Per-Shape Scale Animation

```cpp
class ShapeScaleAnimator {
public:
    ShapeScaleAnimator();

    void SetBody(const Body& body);
    bool Update(float dt);
    void ApplyTo(Body& body) const;

    void SetEnabled(bool enabled);
    bool IsEnabled() const;

private:
    struct ScalableParam {
        std::vector<int> path; // path to the node
        std::string name;
        ShapeType shape_type;
        // Original values for each scalable dimension
        float base_radius;
        float base_height;
        float base_major_radius;
        float base_minor_radius;
    };

    std::vector<ScalableParam> m_params;
    int m_current_shape;
    int m_current_dimension; // which dimension of the current shape is being animated
    float m_elapsed;
    float m_duration; // seconds per dimension
    bool m_enabled;
};
```

The animator cycles through shapes and their individual dimensions (e.g., for a cylinder: first animate radius, then height), scaling from 0.5x to 1.5x of the original value using a sine wave.

### Legacy Format Conversion (v1 → v2)

Best-effort mapping from face indices to parametric coordinates:

| Shape | Face index logic | Parametric equivalent |
|-------|-----------------|----------------------|
| Cylinder | faces[0..N-1] = side quads, faces[N] = top cap, faces[N+1] = bottom cap | top → `{region: "top"}`, bottom → `{region: "bottom"}`, side → `{region: "side", u: index/N}` |
| Cone | faces[0..N-1] = side triangles, faces[N] = base | base → `{region: "base"}`, side → `{region: "side", u: index/N}` |
| Sphere | faces[0..S-1] = top triangles, then quads, then bottom triangles | map to `{region: "surface", u: ..., v: ...}` based on face center position |
| Torus | faces in ring×side order | `{region: "surface", u: ring/rings, v: side/sides}` |

## Tasks

1. [x] Write this spec
2. [ ] Add Capsule to ShapeType enum and ShapeParams
3. [ ] Implement FaceGenerator::GenerateCapsule()
4. [ ] Add AttachmentPoint and ParametricResolver
5. [ ] Update Connection struct to use AttachmentPoint
6. [ ] Update ConnectionSolver for parametric resolution
7. [ ] Update BodyLoader to parse v2 format (with v1 fallback)
8. [ ] Update BodyLoader serialization to emit v2 format
9. [ ] Update ConnectionValidator for new connection model
10. [ ] Implement ShapeScaleAnimator
11. [ ] Integrate ShapeScaleAnimator in Body_Viewer (new hotkey)
12. [ ] Convert all body JSON files to v2 format
13. [ ] Update all tests
14. [ ] Build and verify
