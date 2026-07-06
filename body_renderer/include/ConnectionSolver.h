#pragma once

#include "BodyTypes.h"
#include "FaceGenerator.h"

namespace BodyRenderer {

class ConnectionSolver {
public:
    // Resolves all transforms in the tree (recursive, depth-first)
    void ResolveTree(BodyNode* root) const;

    // Compute transform for a single connection
    Mat4 ComputeTransform(
        const Connection& conn,
        const std::vector<Face>& parent_faces,
        const std::vector<Face>& child_faces
    ) const;

private:
    Mat4 ComputeFaceConnection(const Connection& conn, const Face& parent_face, const Face& child_face) const;
    Mat4 ComputeEdgeConnection(const Connection& conn, const Face& parent_face, const Face& child_face) const;
    Mat4 ComputePointConnection(const Connection& conn, const std::vector<Face>& parent_faces) const;

    Vec3 ComputeFaceCenter(const Face& face) const;
    Vec3 ComputeEdgePoint(const Face& face, float t) const;
};

} // namespace BodyRenderer
