#pragma once

#include "BodyTypes.h"
#include "FaceGenerator.h"
#include "ParametricResolver.h"
#include <vector>

namespace BodyRenderer {

// Describes a connection ring that must be inserted into a shape's geometry
// so that connected shapes share matching N-gon faces at their junction.
struct ConnectionRing {
    Vec3 center;           // Center position of the ring (in shape local space)
    Vec3 normal;           // Normal direction at the ring (outward from shape)
    float radius;          // Radius of the N-gon ring
    int segments;          // Number of vertices in the ring (N of the N-gon)
    int child_index;       // Which child this ring corresponds to (-1 for child-side attachment)
};

// Result of face generation with connection ring modifications applied.
// Contains the modified faces where connection rings have been inserted.
struct MatchedFaces {
    std::vector<Face> faces;                    // All faces for this shape (with ring modifications)
    std::vector<int> connection_face_indices;   // Index into 'faces' for each connection ring face
};

// Computes matching connection face sizes between parent and child shapes.
// At each connection point, both shapes produce an N-gon face of the same size.
// When the matched radius is smaller than the shape's natural radius,
// the shape geometry is deformed (tapered) toward the connection point.
class ConnectionFaceMatcher {
public:
    // Compute the effective connection radius for a shape at a given attachment point.
    float ComputeConnectionRadius(const ShapeParams& shape, const AttachmentPoint& attach) const;

    // Compute the matched connection radius between parent and child.
    float ComputeMatchedRadius(
        const ShapeParams& parent_shape, const AttachmentPoint& parent_attach,
        const ShapeParams& child_shape, const AttachmentPoint& child_attach
    ) const;

    // Compute connection segment count (N of the N-gon).
    int ComputeMatchedSegments(
        const ShapeParams& parent_shape, const AttachmentPoint& parent_attach,
        const ShapeParams& child_shape, const AttachmentPoint& child_attach
    ) const;

    // Generate faces for a node with connection ring modifications.
    // For each connection, the shape geometry is tapered toward the connection
    // point so that the face at the junction matches the required size.
    MatchedFaces GenerateWithConnections(
        const BodyNode& node,
        const std::vector<ConnectionRing>& rings
    ) const;

private:
    // Get the number of segments a shape uses at a given attachment region
    int GetSegmentsAtRegion(const ShapeParams& shape, const AttachmentPoint& attach) const;

    // Compute the natural face radius at a sphere surface point
    float ComputeSphereLocalRadius(const ShapeParams& shape, const AttachmentPoint& attach) const;

    // Taper cylinder geometry toward a connection ring at top or bottom
    void TaperCylinderEnd(
        std::vector<Face>& faces, const ShapeParams& shape,
        const ConnectionRing& ring, int& out_ring_face_index
    ) const;

    // Taper sphere geometry toward a connection ring
    void TaperSphereRegion(
        std::vector<Face>& faces, const ShapeParams& shape,
        const ConnectionRing& ring, int& out_ring_face_index
    ) const;

    // Taper capsule geometry toward a connection ring
    void TaperCapsuleRegion(
        std::vector<Face>& faces, const ShapeParams& shape,
        const ConnectionRing& ring, int& out_ring_face_index
    ) const;

    // Generate an N-gon face centered at a point with given normal and radius
    Face GenerateRingFace(const Vec3& center, const Vec3& normal, float radius, int segments) const;

    ParametricResolver m_resolver;
    FaceGenerator m_faceGen;
};

} // namespace BodyRenderer
