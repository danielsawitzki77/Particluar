#pragma once

#include "BodyTypes.h"
#include "FaceGenerator.h"
#include "ParametricResolver.h"
#include <vector>

namespace BodyRenderer {

// Describes a connection ring — where on a shape a child attaches.
struct ConnectionRing {
    Vec3 center;           // Center position of the ring (in shape local space)
    Vec3 normal;           // Normal direction at the ring (outward from shape)
    float radius;          // Target matched radius for size-matching deformation
    int segments;          // Number of segments (for matched segment computation)
    int child_index;       // Which child this ring corresponds to (-1 for child-side attachment)
    AttachmentPoint attach; // The attachment point used to derive this ring (for grid-index lookup)
};

// Result of face generation with connection modifications applied.
struct MatchedFaces {
    std::vector<Face> faces;                    // All faces for this shape
    std::vector<int> connection_face_indices;   // Index into 'faces' for each connection ring's face
};

// Connection System V3: grid-based face identification + uniform-scale deformation.
//
// Phase 1 (done): Returns unmodified faces, identifies connection faces by proximity.
// Phase 2: Identifies connection faces by grid index (UV → face array index).
// Phase 3: Scales connection face to midpoint radius, propagates shared vertices.
class ConnectionFaceMatcher {
public:
    // Compute the effective connection radius for a shape at a given attachment point.
    float ComputeConnectionRadius(const ShapeParams& shape, const AttachmentPoint& attach) const;

    // Compute the matched connection radius between parent and child (midpoint average).
    float ComputeMatchedRadius(
        const ShapeParams& parent_shape, const AttachmentPoint& parent_attach,
        const ShapeParams& child_shape, const AttachmentPoint& child_attach
    ) const;

    // Compute connection segment count (N of the N-gon).
    int ComputeMatchedSegments(
        const ShapeParams& parent_shape, const AttachmentPoint& parent_attach,
        const ShapeParams& child_shape, const AttachmentPoint& child_attach
    ) const;

    // Generate faces with size-matching deformation at connection points.
    MatchedFaces GenerateWithConnections(
        const BodyNode& node,
        const std::vector<ConnectionRing>& rings
    ) const;

    // Phase 2: Compute the face grid index for a given shape and attachment point.
    // Returns the index into the FaceGenerator::Generate output array.
    int ComputeGridIndex(const ShapeParams& shape, const AttachmentPoint& attach) const;

private:
    int GetSegmentsAtRegion(const ShapeParams& shape, const AttachmentPoint& attach) const;
    float ComputeSphereLocalRadius(const ShapeParams& shape, const AttachmentPoint& attach) const;
    Face GenerateRingFace(const Vec3& center, const Vec3& normal, float radius, int segments) const;

    // Phase 3: Scale a face's vertices uniformly from its center to reach target_radius.
    // Propagates shared vertex moves to neighboring faces.
    void DeformFaceToRadius(std::vector<Face>& faces, int face_index, float target_radius) const;

    ParametricResolver m_resolver;
    FaceGenerator m_faceGen;
};

} // namespace BodyRenderer
