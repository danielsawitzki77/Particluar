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

    // For quads, choose the split diagonal that produces triangles with
    // normals closest to the face normal. Non-planar quads from tapering
    // can produce an inverted triangle if the wrong diagonal is chosen.
    if (n == 4) {
        // Diagonal 0-2: triangles (0,1,2) and (0,2,3)
        Vec3 e1a = face.vertices[1] - face.vertices[0];
        Vec3 e2a = face.vertices[2] - face.vertices[0];
        Vec3 n_a1 = e1a.Cross(e2a);

        Vec3 e1b = face.vertices[2] - face.vertices[0];
        Vec3 e2b = face.vertices[3] - face.vertices[0];
        Vec3 n_a2 = e1b.Cross(e2b);

        float dot_a = 0;
        if (n_a1.Length() > 0.0001f) dot_a += n_a1.Normalized().Dot(face.normal);
        if (n_a2.Length() > 0.0001f) dot_a += n_a2.Normalized().Dot(face.normal);

        // Diagonal 1-3: triangles (0,1,3) and (1,2,3)
        Vec3 e1c = face.vertices[1] - face.vertices[0];
        Vec3 e2c = face.vertices[3] - face.vertices[0];
        Vec3 n_b1 = e1c.Cross(e2c);

        Vec3 e1d = face.vertices[2] - face.vertices[1];
        Vec3 e2d = face.vertices[3] - face.vertices[1];
        Vec3 n_b2 = e1d.Cross(e2d);

        float dot_b = 0;
        if (n_b1.Length() > 0.0001f) dot_b += n_b1.Normalized().Dot(face.normal);
        if (n_b2.Length() > 0.0001f) dot_b += n_b2.Normalized().Dot(face.normal);

        if (dot_a >= dot_b) {
            // Use diagonal 0-2
            Triangle t1;
            t1.v0 = face.vertices[0]; t1.v1 = face.vertices[1]; t1.v2 = face.vertices[2];
            t1.normal = face.normal;
            result.push_back(t1);
            Triangle t2;
            t2.v0 = face.vertices[0]; t2.v1 = face.vertices[2]; t2.v2 = face.vertices[3];
            t2.normal = face.normal;
            result.push_back(t2);
        } else {
            // Use diagonal 1-3
            Triangle t1;
            t1.v0 = face.vertices[0]; t1.v1 = face.vertices[1]; t1.v2 = face.vertices[3];
            t1.normal = face.normal;
            result.push_back(t1);
            Triangle t2;
            t2.v0 = face.vertices[1]; t2.v1 = face.vertices[2]; t2.v2 = face.vertices[3];
            t2.normal = face.normal;
            result.push_back(t2);
        }
        return result;
    }

    // Fan triangulation from vertex 0 for triangles and N-gons (N > 4)
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
