#include "ConnectionSolver.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace BodyRenderer {

static void ResolveChildrenRecursive(BodyNode* node, const FaceGenerator& faceGen, const ConnectionSolver& solver)
{
    std::vector<Face> node_faces = faceGen.Generate(node->shape);
    for (auto& child : node->children) {
        std::vector<Face> child_faces = faceGen.Generate(child.shape);
        child.local_transform = solver.ComputeTransform(child.connection, node_faces, child_faces);
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

Mat4 ConnectionSolver::ComputeTransform(
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

    // Apply rotation_position (additional rotation around parent face normal)
    float rot_angle = conn.rotation * static_cast<float>(M_PI) / 180.0f;
    Mat4 spin = Mat4::RotationAxis(parent_normal, rot_angle);

    // Translation to parent face center
    Mat4 trans = Mat4::Translation(parent_center.x, parent_center.y, parent_center.z);

    return trans * spin * rot;
}

Mat4 ConnectionSolver::ComputeEdgeConnection(const Connection& conn, const Face& parent_face, const Face& /*child_face*/) const
{
    Vec3 edge_point = ComputeEdgePoint(parent_face, conn.offset_u);
    Mat4 trans = Mat4::Translation(edge_point.x, edge_point.y, edge_point.z);

    float rot_angle = conn.rotation * static_cast<float>(M_PI) / 180.0f;
    Vec3 edge_dir(0, 1, 0);
    if (parent_face.vertices.size() >= 2) {
        edge_dir = (parent_face.vertices[1] - parent_face.vertices[0]).Normalized();
    }
    Mat4 spin = Mat4::RotationAxis(edge_dir, rot_angle);

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
