#pragma once

#include "BodyTypes.h"
#include "FaceGenerator.h"
#include "ParametricResolver.h"

namespace BodyRenderer {

class ConnectionSolver {
public:
    // Resolves all transforms in the tree (recursive, depth-first)
    void ResolveTree(BodyNode* root) const;

    // Compute transform for a parametric connection (v2)
    Mat4 ComputeParametricTransform(
        const Connection& conn,
        const ShapeParams& parent_shape,
        const ShapeParams& child_shape
    ) const;

    // Compute transform for a legacy connection (v1 — face indices)
    Mat4 ComputeLegacyTransform(
        const Connection& conn,
        const std::vector<Face>& parent_faces,
        const std::vector<Face>& child_faces
    ) const;

private:
    // Legacy (v1) methods
    Mat4 ComputeFaceConnection(const Connection& conn, const Face& parent_face, const Face& child_face) const;
    Mat4 ComputeEdgeConnection(const Connection& conn, const Face& parent_face, const Face& child_face) const;
    Mat4 ComputePointConnection(const Connection& conn, const std::vector<Face>& parent_faces) const;

    Vec3 ComputeFaceCenter(const Face& face) const;
    Vec3 ComputeEdgePoint(const Face& face, float t) const;

    ParametricResolver m_resolver;
};

} // namespace BodyRenderer
