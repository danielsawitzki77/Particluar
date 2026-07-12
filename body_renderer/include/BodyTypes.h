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
    int heightSegments; // vertical rings for cylinder/capsule (1 = no subdivision)

    // Sphere: radius, latSegments, lonSegments
    int latSegments, lonSegments;

    // Torus: majorRadius, minorRadius, ringSegments, sideSegments
    float majorRadius, minorRadius;
    int ringSegments, sideSegments;

    // Non-uniform row boundaries for height subdivision.
    // If non-empty, these normalized Y-positions (0-1 from bottom to top) define where
    // horizontal ring boundaries are placed. Size should be heightSegments+1 (includes
    // 0.0 and 1.0 endpoints). When empty, uniform spacing is used.
    std::vector<float> rowBoundaries;

    ShapeParams()
        : type(ShapeType::Cylinder)
        , radius(0.5f), height(1.0f), segments(16), heightSegments(1)
        , latSegments(12), lonSegments(16)
        , majorRadius(1.0f), minorRadius(0.25f)
        , ringSegments(16), sideSegments(8)
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
    FaceConnection,
    EdgeConnection,
    PointConnection
};

// ============================================================================
// Connection (v2: parametric, v1: face indices)
// ============================================================================

struct Connection {
    // v2 parametric connection
    AttachmentPoint parentAttach;
    AttachmentPoint childAttach;
    float rotation;           // rotation around connection normal (degrees)

    // v1 legacy fields (used only when loading old format)
    ConnectionType type;
    int parentFaceIndex;
    int childFaceIndex;
    float offsetU, offsetV;
    bool isLegacy;           // true if loaded from v1 format

    Connection()
        : rotation(0.0f)
        , type(ConnectionType::FaceConnection)
        , parentFaceIndex(0)
        , childFaceIndex(0)
        , offsetU(0.5f), offsetV(0.5f)
        , isLegacy(false)
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
    Mat4 localTransform;     // computed from connection by ConnectionSolver
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
    int formatVersion;       // 1 = legacy face indices, 2 = parametric

    Body() : formatVersion(2) {}
};

} // namespace BodyRenderer
