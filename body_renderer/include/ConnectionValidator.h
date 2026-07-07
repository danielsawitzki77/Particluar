#pragma once

#include "BodyTypes.h"
#include "FaceGenerator.h"
#include "ParametricResolver.h"
#include <string>
#include <vector>

namespace BodyRenderer {

struct ValidationResult {
    bool valid;
    std::string error;

    ValidationResult() : valid(true) {}
    ValidationResult(bool v, const std::string& err) : valid(v), error(err) {}
};

class ConnectionValidator {
public:
    // Validate a single legacy connection between parent and child faces
    ValidationResult ValidateConnection(
        const Connection& conn,
        const std::vector<Face>& parent_faces,
        const std::vector<Face>& child_faces,
        const std::string& parent_name,
        const std::string& child_name) const;

    // Validate that child geometry does not penetrate through the shared face
    // into the parent's volume.
    ValidationResult ValidateNoVolumeOverlap(
        const Connection& conn,
        const std::vector<Face>& parent_faces,
        const std::vector<Face>& child_faces,
        const Mat4& child_transform,
        const std::string& parent_name,
        const std::string& child_name) const;

    // Validate a parametric connection (v2)
    ValidationResult ValidateParametricConnection(
        const Connection& conn,
        const ShapeParams& parent_shape,
        const ShapeParams& child_shape,
        const std::string& parent_name,
        const std::string& child_name) const;

    // Validate an entire body tree recursively
    ValidationResult ValidateBody(const Body& body) const;

private:
    ValidationResult ValidateNode(const BodyNode& node, const FaceGenerator& faceGen) const;
    ValidationResult ValidateNodeParametric(const BodyNode& node) const;

    // Check if an AttachRegion is valid for a given shape type
    bool IsRegionValidForShape(AttachRegion region, ShapeType shape_type) const;

    ParametricResolver m_resolver;
};

} // namespace BodyRenderer
