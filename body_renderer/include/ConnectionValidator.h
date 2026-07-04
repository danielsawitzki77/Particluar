#pragma once

#include "BodyTypes.h"
#include "FaceGenerator.h"
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
    // Validate a single connection between parent and child faces
    ValidationResult ValidateConnection(
        const Connection& conn,
        const std::vector<Face>& parent_faces,
        const std::vector<Face>& child_faces,
        const std::string& parent_name,
        const std::string& child_name) const;

    // Validate an entire body tree recursively
    ValidationResult ValidateBody(const Body& body) const;

private:
    ValidationResult ValidateNode(const BodyNode& node, const FaceGenerator& faceGen) const;
};

} // namespace BodyRenderer
