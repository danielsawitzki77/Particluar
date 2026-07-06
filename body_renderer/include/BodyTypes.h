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
    Cone,
    Cylinder,
    Sphere,
    Torus,
    Capsule
};

// Shape parameters — stores fields for all types, only relevant ones used per type
struct ShapeParams {
    ShapeType type;

    // Cone/Cylinder/Capsule: radius, height, segments
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

// ============================================================================
// Attachment regions (parametric connection system v2)
// ============================================================================

enum class AttachRegion {
    // For Sphere/Torus: entire surface
    Surface,
    // For Cylinder/Capsule: specific regions
    Top,
    Bottom,
    Side,
    // For Cone
    Base,
    // For Capsule hemisphere caps
    TopCap,
    BottomCap
};

struct AttachmentPoint {
    AttachRegion region;
    float u;  // 0.0-1.0 first parametric coordinate
    float v;  // 0.0-1.0 second parametric coordinate

    AttachmentPoint() : region(AttachRegion::Top), u(0.5f), v(0.5f) {}
    AttachmentPoint(AttachRegion r, float u_, float v_) : region(r), u(u_), v(v_) {}
};

// ============================================================================
// Connection types (kept for backward compatibility during transition)
// ============================================================================

enum class ConnectionType {
    Face_Connection,
    Edge_Connection,
    Point_Connection
};

// ============================================================================
// Connection (v2: parametric, v1: face indices)
// ============================================================================

struct Connection {
    // v2 parametric connection
    AttachmentPoint parent_attach;
    AttachmentPoint child_attach;
    float rotation;           // rotation around connection normal (degrees)

    // v1 legacy fields (used only when loading old format)
    ConnectionType type;
    int parent_face_index;
    int child_face_index;
    float offset_u, offset_v;
    bool is_legacy;           // true if loaded from v1 format

    Connection()
        : rotation(0.0f)
        , type(ConnectionType::Face_Connection)
        , parent_face_index(0)
        , child_face_index(0)
        , offset_u(0.5f), offset_v(0.5f)
        , is_legacy(false)
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
    int format_version;       // 1 = legacy face indices, 2 = parametric

    Body() : format_version(2) {}
};

} // namespace BodyRenderer
