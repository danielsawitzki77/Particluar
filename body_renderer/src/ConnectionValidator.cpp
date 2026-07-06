#include "ConnectionValidator.h"
#include "ConnectionSolver.h"
#include <sstream>

namespace BodyRenderer {

ValidationResult ConnectionValidator::ValidateConnection(
    const Connection& conn,
    const std::vector<Face>& parent_faces,
    const std::vector<Face>& child_faces,
    const std::string& parent_name,
    const std::string& child_name) const
{
    // Check parent face index bounds
    if (conn.parent_face_index < 0 || conn.parent_face_index >= static_cast<int>(parent_faces.size())) {
        std::ostringstream oss;
        oss << "Connection from '" << parent_name << "' to '" << child_name
            << "': parent_face_index " << conn.parent_face_index
            << " is out of range [0, " << parent_faces.size() - 1 << "]";
        return ValidationResult(false, oss.str());
    }

    // Check child face index bounds
    if (conn.child_face_index < 0 || conn.child_face_index >= static_cast<int>(child_faces.size())) {
        std::ostringstream oss;
        oss << "Connection from '" << parent_name << "' to '" << child_name
            << "': child_face_index " << conn.child_face_index
            << " is out of range [0, " << child_faces.size() - 1 << "]";
        return ValidationResult(false, oss.str());
    }

    // For Face_Connection, verify topology compatibility (same vertex count)
    if (conn.type == ConnectionType::Face_Connection) {
        size_t parent_verts = parent_faces[conn.parent_face_index].vertices.size();
        size_t child_verts = child_faces[conn.child_face_index].vertices.size();

        if (parent_verts != child_verts) {
            std::ostringstream oss;
            oss << "Connection from '" << parent_name << "' to '" << child_name
                << "': face topology mismatch — parent face " << conn.parent_face_index
                << " has " << parent_verts << " vertices, child face " << conn.child_face_index
                << " has " << child_verts << " vertices. "
                << "Faces must share the same polygon topology for subdivision compatibility.";
            return ValidationResult(false, oss.str());
        }
    }

    return ValidationResult(true, "");
}

ValidationResult ConnectionValidator::ValidateNoVolumeOverlap(
    const Connection& conn,
    const std::vector<Face>& parent_faces,
    const std::vector<Face>& child_faces,
    const Mat4& child_transform,
    const std::string& parent_name,
    const std::string& child_name) const
{
    // Volume overlap check only applies to Face_Connection.
    // Edge and Point connections don't define a shared face plane for the constraint.
    if (conn.type != ConnectionType::Face_Connection) {
        return ValidationResult(true, "");
    }

    if (conn.parent_face_index < 0 || conn.parent_face_index >= static_cast<int>(parent_faces.size())) {
        return ValidationResult(true, ""); // bounds already checked in ValidateConnection
    }

    const Face& parent_face = parent_faces[conn.parent_face_index];

    // Compute parent face center and outward normal — these define the half-space
    Vec3 face_center(0, 0, 0);
    for (const Vec3& v : parent_face.vertices) {
        face_center = face_center + v;
    }
    if (!parent_face.vertices.empty()) {
        face_center = face_center * (1.0f / static_cast<float>(parent_face.vertices.size()));
    }
    Vec3 face_normal = parent_face.normal.Normalized();

    // Small tolerance to account for floating point imprecision at the shared face.
    // Vertices exactly on the plane (the connection face itself) are allowed.
    const float epsilon = -1e-4f;

    // Transform all child face vertices into parent-local space and check
    // they lie on the outward side of the parent's connection face plane.
    for (size_t fi = 0; fi < child_faces.size(); ++fi) {
        for (size_t vi = 0; vi < child_faces[fi].vertices.size(); ++vi) {
            Vec3 local_vert = child_faces[fi].vertices[vi];
            Vec3 world_vert = child_transform.TransformPoint(local_vert);

            // Signed distance from the parent face plane
            Vec3 delta = world_vert - face_center;
            float signed_dist = delta.Dot(face_normal);

            if (signed_dist < epsilon) {
                std::ostringstream oss;
                oss << "Connection from '" << parent_name << "' to '" << child_name
                    << "': volume overlap detected — child vertex (face " << fi
                    << ", vertex " << vi << ") penetrates parent face "
                    << conn.parent_face_index << " by "
                    << -signed_dist << " units. "
                    << "Bodies must join at a shared face without overlapping volumes.";
                return ValidationResult(false, oss.str());
            }
        }
    }

    return ValidationResult(true, "");
}

ValidationResult ConnectionValidator::ValidateBody(const Body& body) const
{
    FaceGenerator faceGen;
    return ValidateNode(body.root, faceGen);
}

ValidationResult ConnectionValidator::ValidateNode(const BodyNode& node, const FaceGenerator& faceGen) const
{
    std::vector<Face> node_faces = faceGen.Generate(node.shape);
    ConnectionSolver solver;

    for (const auto& child : node.children) {
        std::vector<Face> child_faces = faceGen.Generate(child.shape);

        // Topology check
        ValidationResult result = ValidateConnection(
            child.connection, node_faces, child_faces, node.name, child.name);

        if (!result.valid) {
            return result;
        }

        // Volume overlap check — compute the child's transform, then verify
        // all child geometry lies on the outward side of the shared face
        Mat4 child_transform = solver.ComputeTransform(child.connection, node_faces, child_faces);

        ValidationResult overlap_result = ValidateNoVolumeOverlap(
            child.connection, node_faces, child_faces, child_transform,
            node.name, child.name);

        if (!overlap_result.valid) {
            return overlap_result;
        }

        // Recurse into child
        ValidationResult child_result = ValidateNode(child, faceGen);
        if (!child_result.valid) {
            return child_result;
        }
    }

    return ValidationResult(true, "");
}

} // namespace BodyRenderer
