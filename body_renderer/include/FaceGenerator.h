#pragma once

#include "BodyTypes.h"
#include <vector>

namespace BodyRenderer {

struct Face {
    std::vector<Vec3> vertices; // ordered CCW when viewed from outside
    Vec3 normal;                // outward-pointing unit normal
};

class FaceGenerator {
public:
    std::vector<Face> Generate(const ShapeParams& shape) const;

private:
    std::vector<Face> GenerateBox(const ShapeParams& s) const;
    std::vector<Face> GenerateCone(const ShapeParams& s) const;
    std::vector<Face> GenerateCylinder(const ShapeParams& s) const;
    std::vector<Face> GenerateSphere(const ShapeParams& s) const;
    std::vector<Face> GenerateTorus(const ShapeParams& s) const;
    std::vector<Face> GenerateFrustum(const ShapeParams& s) const;
};

} // namespace BodyRenderer
