#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <cstring>

namespace BodyRenderer {

// ============================================================================
// Math types
// ============================================================================

struct Vec3 {
    float x, y, z;

    Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& b) const { return Vec3(x + b.x, y + b.y, z + b.z); }
    Vec3 operator-(const Vec3& b) const { return Vec3(x - b.x, y - b.y, z - b.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 operator-() const { return Vec3(-x, -y, -z); }

    float Dot(const Vec3& b) const { return x * b.x + y * b.y + z * b.z; }
    Vec3 Cross(const Vec3& b) const {
        return Vec3(y * b.z - z * b.y,
                    z * b.x - x * b.z,
                    x * b.y - y * b.x);
    }
    float Length() const { return std::sqrt(x * x + y * y + z * z); }
    Vec3 Normalized() const {
        float len = Length();
        if (len < 1e-9f) return Vec3(0, 0, 0);
        return Vec3(x / len, y / len, z / len);
    }
};

// Column-major 4x4 matrix (OpenGL layout)
struct Mat4 {
    float m[16];

    Mat4() { Identity(); }

    void Identity() {
        std::memset(m, 0, sizeof(m));
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    // Translation in elements [12], [13], [14]
    static Mat4 Translation(float x, float y, float z) {
        Mat4 r;
        r.m[12] = x; r.m[13] = y; r.m[14] = z;
        return r;
    }

    static Mat4 RotationX(float radians) {
        Mat4 r;
        float c = std::cos(radians), s = std::sin(radians);
        r.m[5] = c;  r.m[6] = s;
        r.m[9] = -s; r.m[10] = c;
        return r;
    }

    static Mat4 RotationY(float radians) {
        Mat4 r;
        float c = std::cos(radians), s = std::sin(radians);
        r.m[0] = c;  r.m[2] = -s;
        r.m[8] = s;  r.m[10] = c;
        return r;
    }

    static Mat4 RotationZ(float radians) {
        Mat4 r;
        float c = std::cos(radians), s = std::sin(radians);
        r.m[0] = c;  r.m[1] = s;
        r.m[4] = -s; r.m[5] = c;
        return r;
    }

    // Rotation around arbitrary axis (radians)
    static Mat4 RotationAxis(const Vec3& axis, float radians) {
        Vec3 a = axis.Normalized();
        float c = std::cos(radians), s = std::sin(radians);
        float t = 1.0f - c;
        Mat4 r;
        r.m[0]  = t * a.x * a.x + c;
        r.m[1]  = t * a.x * a.y + s * a.z;
        r.m[2]  = t * a.x * a.z - s * a.y;
        r.m[4]  = t * a.x * a.y - s * a.z;
        r.m[5]  = t * a.y * a.y + c;
        r.m[6]  = t * a.y * a.z + s * a.x;
        r.m[8]  = t * a.x * a.z + s * a.y;
        r.m[9]  = t * a.y * a.z - s * a.x;
        r.m[10] = t * a.z * a.z + c;
        return r;
    }

    Mat4 operator*(const Mat4& b) const {
        Mat4 result;
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    sum += m[k * 4 + row] * b.m[col * 4 + k];
                }
                result.m[col * 4 + row] = sum;
            }
        }
        return result;
    }

    Vec3 TransformPoint(const Vec3& p) const {
        float w = m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15];
        if (std::fabs(w) < 1e-9f) w = 1.0f;
        return Vec3(
            (m[0] * p.x + m[4] * p.y + m[8]  * p.z + m[12]) / w,
            (m[1] * p.x + m[5] * p.y + m[9]  * p.z + m[13]) / w,
            (m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]) / w
        );
    }

    Vec3 TransformDirection(const Vec3& d) const {
        return Vec3(
            m[0] * d.x + m[4] * d.y + m[8]  * d.z,
            m[1] * d.x + m[5] * d.y + m[9]  * d.z,
            m[2] * d.x + m[6] * d.y + m[10] * d.z
        );
    }
};

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
