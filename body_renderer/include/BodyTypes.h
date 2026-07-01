#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <cstring>

#include "MathTypes.h"

namespace BodyRenderer {

// Import math types from the shared math library into this namespace
// so all existing code (Vec3, Mat4) continues to work unchanged.
using Particluar::Vec3;
using Particluar::Mat4;

// ============================================================================
// Shape types
// ============================================================================

enum class ShapeType {
    Box,
    Cone,
    Cylinder,
    Sphere,
    Torus,
    Frustum
};

// Shape parameters — stores fields for all types, only relevant ones used per type
struct ShapeParams {
    ShapeType type;

    // Box: width, height, depth
    float width, height, depth;

    // Cone/Cylinder/Frustum: radius, height, segments
    float radius;
    int segments;

    // Sphere: radius, lat_segments, lon_segments
    int lat_segments, lon_segments;

    // Torus: major_radius, minor_radius, ring_segments, side_segments
    float major_radius, minor_radius;
    int ring_segments, side_segments;

    // Frustum: top_radius, bottom_radius (height and segments shared)
    float top_radius, bottom_radius;

    ShapeParams()
        : type(ShapeType::Box)
        , width(1.0f), height(1.0f), depth(1.0f)
        , radius(0.5f), segments(16)
        , lat_segments(12), lon_segments(16)
        , major_radius(1.0f), minor_radius(0.25f)
        , ring_segments(16), side_segments(8)
        , top_radius(0.25f), bottom_radius(0.5f)
    {}
};

// ============================================================================
// Connection types
// ============================================================================

enum class ConnectionType {
    Face_Connection,
    Edge_Connection,
    Point_Connection
};

struct Connection {
    ConnectionType type;
    int parent_face_index;    // which face/edge/point on parent
    int child_face_index;     // which face/edge/point on child (for alignment)
    float offset_u, offset_v; // parametric offset on the face/edge (0.0-1.0)
    float rotation;           // rotation around connection normal (degrees)

    Connection()
        : type(ConnectionType::Face_Connection)
        , parent_face_index(0)
        , child_face_index(0)
        , offset_u(0.5f), offset_v(0.5f)
        , rotation(0.0f)
    {}
};

// ============================================================================
// Body node (shape tree)
// ============================================================================

struct BodyNode {
    std::string name;
    ShapeParams shape;
    Vec3 color;               // RGB 0.0-1.0
    Connection connection;    // how this node attaches to parent (ignored for root)
    Mat4 local_transform;     // computed from connection by ConnectionSolver
    std::vector<BodyNode> children;

    BodyNode() : color(0.7f, 0.7f, 0.7f) {}
};

// ============================================================================
// Material properties
// ============================================================================

struct Material {
    float shininess;
    Vec3 ambient;

    Material() : shininess(32.0f), ambient(0.1f, 0.1f, 0.1f) {}
};

// ============================================================================
// Body (top-level model)
// ============================================================================

struct Body {
    std::string name;
    BodyNode root;
    Material material;
};

} // namespace BodyRenderer
