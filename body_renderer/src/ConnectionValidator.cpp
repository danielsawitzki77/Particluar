#include "ConnectionValidator.h"
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

ValidationResult ConnectionValidator::ValidateBody(const Body& body) const
{
    FaceGenerator faceGen;
    return ValidateNode(body.root, faceGen);
}

ValidationResult ConnectionValidator::ValidateNode(const BodyNode& node, const FaceGenerator& faceGen) const
{
    std::vector<Face> node_faces = faceGen.Generate(node.shape);

    for (const auto& child : node.children) {
        std::vector<Face> child_faces = faceGen.Generate(child.shape);

        ValidationResult result = ValidateConnection(
            child.connection, node_faces, child_faces, node.name, child.name);

        if (!result.valid) {
            return result;
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
