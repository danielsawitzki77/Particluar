#include "ConnectionValidator.h"
#include "ConnectionSolver.h"
#include <cmath>
#include <cstdio>

namespace BodyRenderer {

ValidationResult ConnectionValidator::ValidateBody(const Body& body) const
{
    FaceGenerator faceGen;

    if (body.format_version >= 2) {
        return ValidateNodeParametric(body.root);
    } else {
        return ValidateNode(body.root, faceGen);
    }
}

ValidationResult ConnectionValidator::ValidateNode(const BodyNode& node, const FaceGenerator& faceGen) const
{
    std::vector<Face> node_faces = faceGen.Generate(node.shape);

    for (const auto& child : node.children) {
        std::vector<Face> child_faces = faceGen.Generate(child.shape);

        ValidationResult r = ValidateConnection(child.connection, node_faces, child_faces, node.name, child.name);
        if (!r.valid) return r;

        // Check volume overlap for face connections
        if (child.connection.type == ConnectionType::Face_Connection) {
            // Need to compute the transform to check overlap
            ConnectionSolver solver;
            Mat4 transform = solver.ComputeLegacyTransform(child.connection, node_faces, child_faces);
            ValidationResult overlap = ValidateNoVolumeOverlap(
                child.connection, node_faces, child_faces, transform, node.name, child.name);
            if (!overlap.valid) return overlap;
        }

        // Recurse
        ValidationResult child_result = ValidateNode(child, faceGen);
        if (!child_result.valid) return child_result;
    }

    return ValidationResult();
}

ValidationResult ConnectionValidator::ValidateNodeParametric(const BodyNode& node) const
{
    for (const auto& child : node.children) {
        if (!child.connection.is_legacy) {
            ValidationResult r = ValidateParametricConnection(
                child.connection, node.shape, child.shape, node.name, child.name);
            if (!r.valid) return r;
        }

        // Recurse
        ValidationResult child_result = ValidateNodeParametric(child);
        if (!child_result.valid) return child_result;
    }

    return ValidationResult();
}

ValidationResult ConnectionValidator::ValidateConnection(
    const Connection& conn,
    const std::vector<Face>& parent_faces,
    const std::vector<Face>& child_faces,
    const std::string& parent_name,
    const std::string& child_name) const
{
    if (conn.type == ConnectionType::Face_Connection || conn.type == ConnectionType::Point_Connection) {
        if (conn.parent_face_index < 0 || conn.parent_face_index >= static_cast<int>(parent_faces.size())) {
            return ValidationResult(false,
                "Connection from '" + parent_name + "' to '" + child_name +
                "': parent_face index " + std::to_string(conn.parent_face_index) +
                " out of range [0, " + std::to_string(parent_faces.size() - 1) + "]");
        }
    }

    if (conn.type == ConnectionType::Face_Connection) {
        if (conn.child_face_index < 0 || conn.child_face_index >= static_cast<int>(child_faces.size())) {
            return ValidationResult(false,
                "Connection from '" + parent_name + "' to '" + child_name +
                "': child_face index " + std::to_string(conn.child_face_index) +
                " out of range [0, " + std::to_string(child_faces.size() - 1) + "]");
        }

        // Face topology compatibility check
        size_t parent_verts = parent_faces[conn.parent_face_index].vertices.size();
        size_t child_verts = child_faces[conn.child_face_index].vertices.size();
        if (parent_verts != child_verts) {
            return ValidationResult(false,
                "Connection from '" + parent_name + "' to '" + child_name +
                "': face topology mismatch (parent face has " +
                std::to_string(parent_verts) + " vertices, child face has " +
                std::to_string(child_verts) + " vertices)");
        }
    }

    return ValidationResult();
}

ValidationResult ConnectionValidator::ValidateNoVolumeOverlap(
    const Connection& conn,
    const std::vector<Face>& parent_faces,
    const std::vector<Face>& child_faces,
    const Mat4& child_transform,
    const std::string& parent_name,
    const std::string& child_name) const
{
    if (conn.type != ConnectionType::Face_Connection) {
        return ValidationResult(); // only check for face connections
    }
    if (conn.parent_face_index < 0 || conn.parent_face_index >= static_cast<int>(parent_faces.size())) {
        return ValidationResult(); // invalid index already caught elsewhere
    }

    const Face& parent_face = parent_faces[conn.parent_face_index];
    Vec3 face_center(0, 0, 0);
    for (const Vec3& v : parent_face.vertices) {
        face_center = face_center + v;
    }
    if (!parent_face.vertices.empty()) {
        face_center = face_center * (1.0f / static_cast<float>(parent_face.vertices.size()));
    }

    Vec3 face_normal = parent_face.normal;

    // Check all child vertices: they must be on the outward side of the parent face
    for (const Face& cf : child_faces) {
        for (const Vec3& cv : cf.vertices) {
            Vec3 world_v = child_transform.TransformPoint(cv);
            Vec3 diff = world_v - face_center;
            float dot = diff.Dot(face_normal);
            if (dot < -0.001f) { // small tolerance
                return ValidationResult(false,
                    "volume overlap: child '" + child_name +
                    "' penetrates parent '" + parent_name + "' through connection face");
            }
        }
    }

    return ValidationResult();
}

ValidationResult ConnectionValidator::ValidateParametricConnection(
    const Connection& conn,
    const ShapeParams& parent_shape,
    const ShapeParams& child_shape,
    const std::string& parent_name,
    const std::string& child_name) const
{
    // Validate parent attachment region is valid for parent shape type
    if (!IsRegionValidForShape(conn.parent_attach.region, parent_shape.type)) {
        return ValidationResult(false,
            "Connection from '" + parent_name + "' to '" + child_name +
            "': parent attachment region is not valid for shape type");
    }

    // Validate child attachment region is valid for child shape type
    if (!IsRegionValidForShape(conn.child_attach.region, child_shape.type)) {
        return ValidationResult(false,
            "Connection from '" + parent_name + "' to '" + child_name +
            "': child attachment region is not valid for shape type");
    }

    // Validate u/v are in range (already clamped during parsing, but double-check)
    if (conn.parent_attach.u < 0.0f || conn.parent_attach.u > 1.0f ||
        conn.parent_attach.v < 0.0f || conn.parent_attach.v > 1.0f) {
        return ValidationResult(false,
            "Connection from '" + parent_name + "' to '" + child_name +
            "': parent attachment u/v out of [0,1] range");
    }
    if (conn.child_attach.u < 0.0f || conn.child_attach.u > 1.0f ||
        conn.child_attach.v < 0.0f || conn.child_attach.v > 1.0f) {
        return ValidationResult(false,
            "Connection from '" + parent_name + "' to '" + child_name +
            "': child attachment u/v out of [0,1] range");
    }

    // Warn (not reject) if sphere attachment v lands in a pole region.
    // Sphere poles produce triangles; only mid-band latitudes produce quads.
    auto warnSpherePole = [&](const char* role, const ShapeParams& shape, float v) {
        if (shape.type == ShapeType::Sphere && shape.lat_segments > 0) {
            float pole_threshold = 1.0f / static_cast<float>(shape.lat_segments);
            if (v < pole_threshold || v > (1.0f - pole_threshold)) {
                fprintf(stderr, "[ConnectionValidator] WARNING: %s '%s' -> '%s': "
                    "sphere %s attachment v=%.3f lands in pole region (triangles). "
                    "Use v in [%.3f, %.3f] for quad faces.\n",
                    role, parent_name.c_str(), child_name.c_str(), role,
                    v, pole_threshold, 1.0f - pole_threshold);
            }
        }
    };
    warnSpherePole("parent", parent_shape, conn.parent_attach.v);
    warnSpherePole("child", child_shape, conn.child_attach.v);

    // Warn if rotation is specified for side/surface connections (has no effect)
    if (conn.rotation != 0.0f) {
        bool is_side = (conn.child_attach.region == AttachRegion::Side ||
                        conn.parent_attach.region == AttachRegion::Surface ||
                        conn.parent_attach.region == AttachRegion::Side);
        if (is_side) {
            fprintf(stderr, "[ConnectionValidator] WARNING: '%s' -> '%s': "
                "rotation=%.1f specified for side/surface connection (ignored — "
                "face grid determines orientation for quad connections).\n",
                parent_name.c_str(), child_name.c_str(), conn.rotation);
        }
    }

    return ValidationResult();
}

bool ConnectionValidator::IsRegionValidForShape(AttachRegion region, ShapeType shape_type) const
{
    switch (shape_type) {
    case ShapeType::Sphere:
        return region == AttachRegion::Surface;
    case ShapeType::Cylinder:
        return region == AttachRegion::Top || region == AttachRegion::Bottom || region == AttachRegion::Side;
    case ShapeType::Cone:
        return region == AttachRegion::Base || region == AttachRegion::Side;
    case ShapeType::Torus:
        return region == AttachRegion::Surface;
    case ShapeType::Capsule:
        return region == AttachRegion::Top || region == AttachRegion::TopCap ||
               region == AttachRegion::Bottom || region == AttachRegion::BottomCap ||
               region == AttachRegion::Side;
    }
    return false;
}

} // namespace BodyRenderer
