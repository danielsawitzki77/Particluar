#include "ConnectionSolver.h"
#include "ConnectionFaceMatcher.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace BodyRenderer {

static void ResolveChildrenRecursive(BodyNode* node, const FaceGenerator& faceGen, const ConnectionSolver& solver)
{
    for (auto& child : node->children) {
        if (child.connection.is_legacy) {
            // Legacy v1: use face indices
            std::vector<Face> node_faces = faceGen.Generate(node->shape);
            std::vector<Face> child_faces = faceGen.Generate(child.shape);
            child.local_transform = solver.ComputeLegacyTransform(child.connection, node_faces, child_faces);
        } else {
            // v2 parametric
            child.local_transform = solver.ComputeParametricTransform(child.connection, node->shape, child.shape);
        }
        ResolveChildrenRecursive(&child, faceGen, solver);
    }
}

void ConnectionSolver::ResolveTree(BodyNode* root) const
{
    if (!root) return;

    // Root has identity transform
    root->local_transform.Identity();

    FaceGenerator faceGen;
    ResolveChildrenRecursive(root, faceGen, *this);
}

// ============================================================================
// Parametric (v2) transform computation
// ============================================================================

Mat4 ConnectionSolver::ComputeParametricTransform(
    const Connection& conn,
    const ShapeParams& parent_shape,
    const ShapeParams& child_shape) const
{
    // Resolve parent and child attachment points on continuous surface
    SurfacePoint parent_pt = m_resolver.Resolve(parent_shape, conn.parent_attach);
    SurfacePoint child_pt = m_resolver.Resolve(child_shape, conn.child_attach);

    // Snap to actual face center for shapes with discrete face grids.
    // This ensures the child is positioned at the face center (where the
    // actual geometry lives), not at an arbitrary continuous surface point.
    FaceGenerator faceGen;
    ConnectionFaceMatcher faceMatcher;

    // Get parent face center
    {
        std::vector<Face> parent_faces = faceGen.Generate(parent_shape);
        int grid_idx = faceMatcher.ComputeGridIndex(parent_shape, conn.parent_attach);
        if (grid_idx >= 0 && grid_idx < static_cast<int>(parent_faces.size())) {
            Vec3 fc(0, 0, 0);
            for (const auto& v : parent_faces[grid_idx].vertices) fc = fc + v;
            fc = fc * (1.0f / parent_faces[grid_idx].vertices.size());
            parent_pt.position = fc;
            // Keep the resolved normal (it's smooth), but use face normal for flat-shading alignment
            parent_pt.normal = parent_faces[grid_idx].normal;
        }
    }

    // Get child face center
    {
        std::vector<Face> child_faces = faceGen.Generate(child_shape);
        int grid_idx = faceMatcher.ComputeGridIndex(child_shape, conn.child_attach);
        if (grid_idx >= 0 && grid_idx < static_cast<int>(child_faces.size())) {
            Vec3 fc(0, 0, 0);
            for (const auto& v : child_faces[grid_idx].vertices) fc = fc + v;
            fc = fc * (1.0f / child_faces[grid_idx].vertices.size());
            child_pt.position = fc;
            child_pt.normal = child_faces[grid_idx].normal;
        }
    }

    // The child's attachment normal should face opposite to parent's attachment normal
    Vec3 from = child_pt.normal.Normalized();
    Vec3 to = (parent_pt.normal * (-1.0f)).Normalized();

    Mat4 rot;
    float dot = from.Dot(to);
    if (dot > 0.9999f) {
        rot.Identity();
    } else if (dot < -0.9999f) {
        // 180-degree rotation around any perpendicular axis
        Vec3 perp(1, 0, 0);
        if (std::fabs(from.Dot(perp)) > 0.9f) perp = Vec3(0, 1, 0);
        Vec3 axis = from.Cross(perp).Normalized();
        rot = Mat4::RotationAxis(axis, static_cast<float>(M_PI));
    } else {
        Vec3 axis = from.Cross(to).Normalized();
        float angle = std::acos(dot);
        rot = Mat4::RotationAxis(axis, angle);
    }

    // Apply spin rotation around parent normal.
    // For side↔surface (quad↔quad) connections, compute alignment rotation
    // so the child's face edges match the parent's face edges.
    // For cap↔cap connections, use the specified rotation from JSON.
    float rot_angle = 0.0f;
    bool is_side_connection = 
        (conn.child_attach.region == AttachRegion::Side ||
         conn.parent_attach.region == AttachRegion::Surface ||
         conn.parent_attach.region == AttachRegion::Side);
    
    if (!is_side_connection) {
        rot_angle = conn.rotation * static_cast<float>(M_PI) / 180.0f;
    } else {
        // Compute alignment rotation from face edge directions.
        // Get parent face's first edge direction (tangent along the face grid)
        std::vector<Face> parent_faces = faceGen.Generate(parent_shape);
        int parent_grid_idx = faceMatcher.ComputeGridIndex(parent_shape, conn.parent_attach);
        if (parent_grid_idx >= 0 && parent_grid_idx < static_cast<int>(parent_faces.size()) &&
            parent_faces[parent_grid_idx].vertices.size() >= 2) {
            Vec3 parent_edge = (parent_faces[parent_grid_idx].vertices[1] - 
                                parent_faces[parent_grid_idx].vertices[0]).Normalized();
            
            // Get child face's first edge direction after applying rot
            std::vector<Face> child_faces = faceGen.Generate(child_shape);
            int child_grid_idx = faceMatcher.ComputeGridIndex(child_shape, conn.child_attach);
            if (child_grid_idx >= 0 && child_grid_idx < static_cast<int>(child_faces.size()) &&
                child_faces[child_grid_idx].vertices.size() >= 2) {
                Vec3 child_edge_local = (child_faces[child_grid_idx].vertices[1] - 
                                         child_faces[child_grid_idx].vertices[0]).Normalized();
                // Transform child edge by the normal-alignment rotation
                Vec3 child_edge = rot.TransformDirection(child_edge_local).Normalized();
                
                // Project both edges onto the plane perpendicular to parent normal
                Vec3 N = parent_pt.normal.Normalized();
                Vec3 pe = (parent_edge - N * parent_edge.Dot(N)).Normalized();
                Vec3 ce = (child_edge - N * child_edge.Dot(N)).Normalized();
                
                // Compute angle between them
                if (pe.Length() > 0.1f && ce.Length() > 0.1f) {
                    pe = pe.Normalized();
                    ce = ce.Normalized();
                    float cos_a = pe.Dot(ce);
                    if (cos_a > 1.0f) cos_a = 1.0f;
                    if (cos_a < -1.0f) cos_a = -1.0f;
                    rot_angle = std::acos(cos_a);
                    // Determine sign using cross product
                    Vec3 cross = ce.Cross(pe);
                    if (cross.Dot(N) < 0) rot_angle = -rot_angle;
                }
            }
        }
    }
    Mat4 spin = Mat4::RotationAxis(parent_pt.normal, rot_angle);

    // Orientation = spin * rot
    Mat4 orientation = spin * rot;

    // After rotating the child, its attachment point moves
    Vec3 rotated_child_attach = orientation.TransformPoint(child_pt.position);

    // Translate so the child's attachment point lands on the parent's attachment point
    Vec3 final_pos = parent_pt.position - rotated_child_attach;
    Mat4 trans = Mat4::Translation(final_pos.x, final_pos.y, final_pos.z);

    return trans * spin * rot;
}

// ============================================================================
// Legacy (v1) transform computation
// ============================================================================

Mat4 ConnectionSolver::ComputeLegacyTransform(
    const Connection& conn,
    const std::vector<Face>& parent_faces,
    const std::vector<Face>& child_faces) const
{
    switch (conn.type) {
    case ConnectionType::Face_Connection: {
        if (conn.parent_face_index < 0 || conn.parent_face_index >= static_cast<int>(parent_faces.size()))
            return Mat4();
        if (conn.child_face_index < 0 || conn.child_face_index >= static_cast<int>(child_faces.size()))
            return Mat4();
        return ComputeFaceConnection(conn, parent_faces[conn.parent_face_index], child_faces[conn.child_face_index]);
    }
    case ConnectionType::Edge_Connection: {
        if (conn.parent_face_index < 0 || conn.parent_face_index >= static_cast<int>(parent_faces.size()))
            return Mat4();
        if (conn.child_face_index < 0 || conn.child_face_index >= static_cast<int>(child_faces.size()))
            return Mat4();
        return ComputeEdgeConnection(conn, parent_faces[conn.parent_face_index], child_faces[conn.child_face_index]);
    }
    case ConnectionType::Point_Connection: {
        return ComputePointConnection(conn, parent_faces);
    }
    }
    return Mat4();
}

Vec3 ConnectionSolver::ComputeFaceCenter(const Face& face) const
{
    Vec3 center(0, 0, 0);
    if (face.vertices.empty()) return center;
    for (const Vec3& v : face.vertices) {
        center = center + v;
    }
    float inv = 1.0f / static_cast<float>(face.vertices.size());
    return center * inv;
}

Vec3 ConnectionSolver::ComputeEdgePoint(const Face& face, float t) const
{
    if (face.vertices.size() < 2) return Vec3(0, 0, 0);
    const Vec3& a = face.vertices[0];
    const Vec3& b = face.vertices[1];
    return a + (b - a) * t;
}

Mat4 ConnectionSolver::ComputeFaceConnection(const Connection& conn, const Face& parent_face, const Face& child_face) const
{
    Vec3 parent_center = ComputeFaceCenter(parent_face);
    Vec3 parent_normal = parent_face.normal;
    Vec3 child_normal = child_face.normal;

    // Rotate child so its connection face normal points opposite to parent face normal
    Vec3 from = child_normal.Normalized();
    Vec3 to = (parent_normal * (-1.0f)).Normalized();

    Mat4 rot;
    float dot = from.Dot(to);
    if (dot > 0.9999f) {
        rot.Identity();
    } else if (dot < -0.9999f) {
        Vec3 perp(1, 0, 0);
        if (std::fabs(from.Dot(perp)) > 0.9f) perp = Vec3(0, 1, 0);
        Vec3 axis = from.Cross(perp).Normalized();
        rot = Mat4::RotationAxis(axis, static_cast<float>(M_PI));
    } else {
        Vec3 axis = from.Cross(to).Normalized();
        float angle = std::acos(dot);
        rot = Mat4::RotationAxis(axis, angle);
    }

    // Apply rotation around parent face normal
    float rot_angle = conn.rotation * static_cast<float>(M_PI) / 180.0f;
    Mat4 spin = Mat4::RotationAxis(parent_normal, rot_angle);

    // Compute the child's connection face center in child local space
    Vec3 child_face_center = ComputeFaceCenter(child_face);

    // After spin*rot is applied to the child, the connection face center moves to:
    Mat4 orientation = spin * rot;
    Vec3 rotated_child_center = orientation.TransformPoint(child_face_center);

    // Offset translation so the child's connection face sits flush on the parent face
    Vec3 final_pos = parent_center - rotated_child_center;
    Mat4 trans = Mat4::Translation(final_pos.x, final_pos.y, final_pos.z);

    return trans * spin * rot;
}

Mat4 ConnectionSolver::ComputeEdgeConnection(const Connection& conn, const Face& parent_face, const Face& child_face) const
{
    Vec3 edge_point = ComputeEdgePoint(parent_face, conn.offset_u);

    float rot_angle = conn.rotation * static_cast<float>(M_PI) / 180.0f;
    Vec3 edge_dir(0, 1, 0);
    if (parent_face.vertices.size() >= 2) {
        edge_dir = (parent_face.vertices[1] - parent_face.vertices[0]).Normalized();
    }
    Mat4 spin = Mat4::RotationAxis(edge_dir, rot_angle);

    // Offset child so its connection face center lands on the edge point
    Vec3 child_face_center = ComputeFaceCenter(child_face);
    Vec3 rotated_child_center = spin.TransformPoint(child_face_center);
    Vec3 final_pos = edge_point - rotated_child_center;
    Mat4 trans = Mat4::Translation(final_pos.x, final_pos.y, final_pos.z);

    return trans * spin;
}

Mat4 ConnectionSolver::ComputePointConnection(const Connection& conn, const std::vector<Face>& parent_faces) const
{
    Vec3 point(0, 0, 0);
    if (conn.parent_face_index >= 0 && conn.parent_face_index < static_cast<int>(parent_faces.size())) {
        point = ComputeFaceCenter(parent_faces[conn.parent_face_index]);
    }

    float rot_angle = conn.rotation * static_cast<float>(M_PI) / 180.0f;
    Mat4 spin = Mat4::RotationY(rot_angle);

    return Mat4::Translation(point.x, point.y, point.z) * spin;
}

} // namespace BodyRenderer
