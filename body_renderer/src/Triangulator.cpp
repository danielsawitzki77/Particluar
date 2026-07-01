#include "Triangulator.h"

namespace BodyRenderer {

std::vector<Triangle> Triangulator::Triangulate(const std::vector<Face>& faces) const
{
    std::vector<Triangle> result;
    for (const Face& f : faces) {
        auto tris = TriangulateFace(f);
        result.insert(result.end(), tris.begin(), tris.end());
    }
    return result;
}

std::vector<Triangle> Triangulator::TriangulateFace(const Face& face) const
{
    std::vector<Triangle> result;
    int n = static_cast<int>(face.vertices.size());

    // Degenerate face
    if (n < 3) return result;

    // Fan triangulation from vertex 0
    for (int i = 1; i < n - 1; ++i) {
        Triangle tri;
        tri.v0 = face.vertices[0];
        tri.v1 = face.vertices[i];
        tri.v2 = face.vertices[i + 1];
        tri.normal = face.normal;
        result.push_back(tri);
    }

    return result;
}

} // namespace BodyRenderer
