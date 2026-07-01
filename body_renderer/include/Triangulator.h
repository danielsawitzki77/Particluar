#pragma once

#include "FaceGenerator.h"
#include <vector>

namespace BodyRenderer {

struct Triangle {
    Vec3 v0, v1, v2;
    Vec3 normal; // flat-shading: same as face normal
};

class Triangulator {
public:
    // Fan decomposition: for N-vertex face, produces N-2 triangles
    std::vector<Triangle> Triangulate(const std::vector<Face>& faces) const;
    std::vector<Triangle> TriangulateFace(const Face& face) const;
};

} // namespace BodyRenderer
