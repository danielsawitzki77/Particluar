#include "ConnectionFaceMatcher.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace BodyRenderer {

// ============================================================================
// Connection radius computation
// ============================================================================

float ConnectionFaceMatcher::ComputeConnectionRadius(const ShapeParams& shape, const AttachmentPoint& attach) const
{
    switch (shape.type) {
    case ShapeType::Cylinder:
        return shape.radius;
    case ShapeType::Sphere:
        return ComputeSphereLocalRadius(shape, attach);
    case ShapeType::Capsule:
        return shape.radius;
    case ShapeType::Cone:
        if (attach.region == AttachRegion::Base)
            return shape.radius;
        return shape.radius * (1.0f - attach.v);
    case ShapeType::Torus:
        return shape.minor_radius;
    }
    return 0.1f;
}

float ConnectionFaceMatcher::ComputeSphereLocalRadius(const ShapeParams& shape, const AttachmentPoint& attach) const
{
    float phi = attach.v * static_cast<float>(M_PI);
    float ring_radius = shape.radius * std::sin(phi);
    if (ring_radius < shape.radius * 0.05f)
        ring_radius = shape.radius * 0.15f;
    return ring_radius;
}

float ConnectionFaceMatcher::ComputeMatchedRadius(
    const ShapeParams& parent_shape, const AttachmentPoint& parent_attach,
    const ShapeParams& child_shape, const AttachmentPoint& child_attach) const
{
    float parent_r = ComputeConnectionRadius(parent_shape, parent_attach);
    float child_r = ComputeConnectionRadius(child_shape, child_attach);
    float matched = (parent_r + child_r) * 0.5f;
    if (matched < 0.01f) matched = 0.01f;
    return matched;
}

int ConnectionFaceMatcher::GetSegmentsAtRegion(const ShapeParams& shape, const AttachmentPoint& /*attach*/) const
{
    switch (shape.type) {
    case ShapeType::Cylinder:
    case ShapeType::Capsule:
    case ShapeType::Cone:
        return shape.segments;
    case ShapeType::Sphere:
        return shape.lon_segments;
    case ShapeType::Torus:
        return shape.side_segments;
    }
    return 8;
}

int ConnectionFaceMatcher::ComputeMatchedSegments(
    const ShapeParams& parent_shape, const AttachmentPoint& parent_attach,
    const ShapeParams& child_shape, const AttachmentPoint& child_attach) const
{
    int parent_n = GetSegmentsAtRegion(parent_shape, parent_attach);
    int child_n = GetSegmentsAtRegion(child_shape, child_attach);
    return std::max(parent_n, child_n);
}

// ============================================================================
// Phase 2: Grid-based face identification
//
// Maps UV parameters to deterministic face indices based on the known
// emission order of FaceGenerator::Generate for each primitive type.
// ============================================================================

int ConnectionFaceMatcher::ComputeGridIndex(const ShapeParams& shape, const AttachmentPoint& attach) const
{
    // Clamp UV to [0, 1)
    float u = std::max(0.0f, std::min(0.999f, attach.u));
    float v = std::max(0.0f, std::min(0.999f, attach.v));

    switch (shape.type) {
    case ShapeType::Cylinder: {
        int N = shape.segments;
        int H = std::max(1, shape.height_segments);
        if (attach.region == AttachRegion::Top)
            return N * H;  // Top cap is after all lateral quads
        if (attach.region == AttachRegion::Bottom)
            return N * H + 1;  // Bottom cap is after top cap
        // Side: lateral quads in row-major order (row 0 = bottom)
        int col = static_cast<int>(u * N) % N;
        int row = static_cast<int>(v * H) % H;
        return row * N + col;
    }
    case ShapeType::Cone: {
        int N = shape.segments;
        if (attach.region == AttachRegion::Base)
            return N;  // Base cap is after N lateral triangles
        // Side: lateral triangles
        int col = static_cast<int>(u * N) % N;
        return col;
    }
    case ShapeType::Torus: {
        int R = shape.ring_segments;
        int T = shape.side_segments;
        int ring_idx = static_cast<int>(u * R) % R;
        int tube_idx = static_cast<int>(v * T) % T;
        return ring_idx * T + tube_idx;
    }
    case ShapeType::Sphere: {
        int S = shape.lon_segments;
        int T = shape.lat_segments;
        int col = static_cast<int>(u * S) % S;
        float stack_f = v * T;
        if (stack_f < 1.0f) {
            // North pole triangle
            return col;
        }
        if (stack_f >= static_cast<float>(T - 1)) {
            // South pole triangle
            return S + S * (T - 2) + col;
        }
        // Mid-band quad
        int row = static_cast<int>(stack_f) - 1;
        if (row < 0) row = 0;
        if (row >= T - 2) row = T - 3;
        return S + row * S + col;
    }
    case ShapeType::Capsule: {
        int N = shape.segments;
        int H = std::max(1, shape.height_segments);
        int hemi_stacks = N / 2;
        if (hemi_stacks < 2) hemi_stacks = 2;

        // Face order: top pole tris (N), top hemi quads (N*(hemi-1)),
        //             cylinder lateral (N*H), bottom hemi quads (N*(hemi-1)), bottom pole tris (N)
        int top_pole_count = N;
        int top_hemi_count = N * (hemi_stacks - 1);
        int cyl_count = N * H;

        if (attach.region == AttachRegion::TopCap || attach.region == AttachRegion::Top) {
            // v=0 is pole, v=1 is equator
            float stack_f = v * hemi_stacks;
            int col = static_cast<int>(u * N) % N;
            if (stack_f < 1.0f) {
                return col; // top pole triangle
            }
            int row = static_cast<int>(stack_f) - 1;
            if (row >= hemi_stacks - 1) row = hemi_stacks - 2;
            return top_pole_count + row * N + col;
        }
        if (attach.region == AttachRegion::Side) {
            int col = static_cast<int>(u * N) % N;
            int row = static_cast<int>(v * H) % H;
            return top_pole_count + top_hemi_count + row * N + col;
        }
        if (attach.region == AttachRegion::BottomCap || attach.region == AttachRegion::Bottom) {
            int col = static_cast<int>(u * N) % N;
            float stack_f = v * hemi_stacks;
            int bottom_hemi_offset = top_pole_count + top_hemi_count + cyl_count;
            if (stack_f >= static_cast<float>(hemi_stacks - 1)) {
                // bottom pole triangle
                return bottom_hemi_offset + N * (hemi_stacks - 1) + col;
            }
            int row = static_cast<int>(stack_f);
            if (row >= hemi_stacks - 1) row = hemi_stacks - 2;
            return bottom_hemi_offset + row * N + col;
        }
        return 0;
    }
    }
    return 0;
}

// ============================================================================
// Phase 3: Size-matching deformation
// ============================================================================

void ConnectionFaceMatcher::DeformFaceToRadius(
    std::vector<Face>& faces, int face_index, float target_radius) const
{
    if (face_index < 0 || face_index >= static_cast<int>(faces.size()))
        return;

    Face& cf = faces[face_index];
    int n_verts = static_cast<int>(cf.vertices.size());
    if (n_verts < 3) return;

    // Compute face center
    Vec3 face_center(0, 0, 0);
    for (const auto& v : cf.vertices)
        face_center = face_center + v;
    face_center = face_center * (1.0f / n_verts);

    // Compute current average radius
    float current_radius = 0.0f;
    for (const auto& v : cf.vertices)
        current_radius += (v - face_center).Length();
    current_radius /= n_verts;

    // Skip if already matching or degenerate
    if (current_radius < 1e-6f) return;
    if (std::fabs(current_radius - target_radius) < 1e-5f) return;

    float scale_factor = target_radius / current_radius;

    // Save old positions for shared-vertex propagation
    std::vector<Vec3> old_positions(n_verts);
    for (int i = 0; i < n_verts; ++i)
        old_positions[i] = cf.vertices[i];

    // Scale connection face vertices uniformly from face center
    for (int i = 0; i < n_verts; ++i) {
        Vec3 from_center = cf.vertices[i] - face_center;
        cf.vertices[i] = face_center + from_center * scale_factor;
    }

    // Propagate shared vertex positions to neighboring faces
    for (int fi = 0; fi < static_cast<int>(faces.size()); ++fi) {
        if (fi == face_index) continue;
        for (auto& v : faces[fi].vertices) {
            for (int ci = 0; ci < n_verts; ++ci) {
                if ((v - old_positions[ci]).Length() < 1e-5f) {
                    v = cf.vertices[ci];
                    break;
                }
            }
        }
    }

    // Recompute normal for the connection face
    if (n_verts >= 3) {
        Vec3 e1 = cf.vertices[1] - cf.vertices[0];
        Vec3 e2 = cf.vertices[2] - cf.vertices[0];
        Vec3 n = e1.Cross(e2);
        if (n.Length() > 1e-6f) {
            n = n.Normalized();
            if (n.Dot(cf.normal) < 0) n = n * (-1.0f);
            cf.normal = n;
        }
    }
}

// ============================================================================
// Utility
// ============================================================================

Face ConnectionFaceMatcher::GenerateRingFace(const Vec3& center, const Vec3& normal, float radius, int segments) const
{
    Face face;
    face.normal = normal.Normalized();
    face.vertices.resize(segments);

    Vec3 N = face.normal;
    Vec3 U, V;
    if (std::fabs(N.y) < 0.9f)
        U = Vec3(0, 1, 0).Cross(N).Normalized();
    else
        U = Vec3(1, 0, 0).Cross(N).Normalized();
    V = N.Cross(U).Normalized();

    for (int i = 0; i < segments; ++i) {
        float angle = 2.0f * static_cast<float>(M_PI) * i / segments;
        face.vertices[i] = center + U * (radius * std::cos(angle)) + V * (radius * std::sin(angle));
    }
    return face;
}

// ============================================================================
// Main entry: generate faces with connection-point size matching
// ============================================================================

MatchedFaces ConnectionFaceMatcher::GenerateWithConnections(
    const BodyNode& node,
    const std::vector<ConnectionRing>& rings) const
{
    MatchedFaces result;
    result.faces = m_faceGen.Generate(node.shape);

    if (rings.empty()) {
        return result;
    }

    int total_faces = static_cast<int>(result.faces.size());

    for (size_t ri = 0; ri < rings.size(); ++ri) {
        const auto& ring = rings[ri];

        // Phase 2: Use grid-index to identify the connection face
        int conn_face = ComputeGridIndex(node.shape, ring.attach);

        // Bounds check
        if (conn_face < 0 || conn_face >= total_faces) {
            // Fallback to proximity search if grid index is out of bounds
            conn_face = -1;
            float best_dist = 1e9f;
            for (int i = 0; i < total_faces; ++i) {
                Vec3 center(0, 0, 0);
                for (const auto& v : result.faces[i].vertices) center = center + v;
                center = center * (1.0f / result.faces[i].vertices.size());
                float d = (center - ring.center).Length();
                if (d < best_dist) { best_dist = d; conn_face = i; }
            }
        }

        // Phase 3: Apply size-matching deformation
        if (conn_face >= 0) {
            float natural_radius = ComputeConnectionRadius(node.shape, ring.attach);
            if (std::fabs(ring.radius - natural_radius) > 1e-4f) {
                DeformFaceToRadius(result.faces, conn_face, ring.radius);
            }
        }

        result.connection_face_indices.push_back(conn_face);
    }

    return result;
}

} // namespace BodyRenderer
