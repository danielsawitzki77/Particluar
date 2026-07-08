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
        if (attach.region == AttachRegion::Top || attach.region == AttachRegion::Bottom)
            return shape.radius;
        return shape.radius; // Side
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
    // Meet in the middle
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
// V3 Phase 1: No deformation. Return unmodified faces.
//
// Each body renders as a complete closed solid with its original geometry.
// The ConnectionSolver handles positioning so bodies touch at their
// connection faces. No mesh modification happens here.
// ============================================================================

MatchedFaces ConnectionFaceMatcher::GenerateWithConnections(
    const BodyNode& node,
    const std::vector<ConnectionRing>& rings) const
{
    MatchedFaces result;

    // Generate base faces — completely unmodified
    result.faces = m_faceGen.Generate(node.shape);

    // Record which face is closest to each connection ring center
    // (for future Phase 3 size matching — not used for deformation yet)
    for (size_t ri = 0; ri < rings.size(); ++ri) {
        const auto& ring = rings[ri];
        int closest = -1;
        float best_dist = 1e9f;
        for (int i = 0; i < static_cast<int>(result.faces.size()); ++i) {
            Vec3 center(0, 0, 0);
            for (const auto& v : result.faces[i].vertices) center = center + v;
            center = center * (1.0f / result.faces[i].vertices.size());
            float d = (center - ring.center).Length();
            if (d < best_dist) { best_dist = d; closest = i; }
        }
        result.connection_face_indices.push_back(closest);
    }

    return result;
}

} // namespace BodyRenderer
