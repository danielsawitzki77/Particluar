#include "FaceGenerator.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace BodyRenderer {

std::vector<Face> FaceGenerator::Generate(const ShapeParams& shape) const
{
    switch (shape.type) {
    case ShapeType::Cone:     return GenerateCone(shape);
    case ShapeType::Cylinder: return GenerateCylinder(shape);
    case ShapeType::Sphere:   return GenerateSphere(shape);
    case ShapeType::Torus:    return GenerateTorus(shape);
    case ShapeType::Capsule:  return GenerateCapsule(shape);
    }
    return {};
}

std::vector<Face> FaceGenerator::GenerateCone(const ShapeParams& s) const
{
    std::vector<Face> faces;
    int n = s.segments;
    float r = s.radius;
    float h = s.height;
    float hh = h * 0.5f;

    Vec3 tip(0, hh, 0);

    // Generate base circle vertices (y = -hh)
    std::vector<Vec3> base_verts(n);
    for (int i = 0; i < n; ++i) {
        float angle = 2.0f * static_cast<float>(M_PI) * i / n;
        base_verts[i] = Vec3(r * std::cos(angle), -hh, r * std::sin(angle));
    }

    // Lateral triangular faces
    for (int i = 0; i < n; ++i) {
        int next = (i + 1) % n;
        Face f;
        f.vertices = {base_verts[i], base_verts[next], tip};

        // Normal: cross of two edges, pointing outward
        Vec3 e1 = base_verts[next] - base_verts[i];
        Vec3 e2 = tip - base_verts[i];
        f.normal = e1.Cross(e2).Normalized();
        faces.push_back(f);
    }

    // Base face (N-gon, CCW when viewed from below, i.e., normal pointing -Y)
    Face base;
    base.normal = Vec3(0, -1, 0);
    // Reverse winding for bottom face
    for (int i = n - 1; i >= 0; --i) {
        base.vertices.push_back(base_verts[i]);
    }
    faces.push_back(base);

    return faces;
}

std::vector<Face> FaceGenerator::GenerateCylinder(const ShapeParams& s) const
{
    std::vector<Face> faces;
    int n = s.segments;
    float r = s.radius;
    float hh = s.height * 0.5f;
    int h_segs = s.height_segments;
    if (h_segs < 1) h_segs = 1;

    // Generate ring vertices: (h_segs + 1) rings from bottom to top
    // ring_verts[row][col], row 0 = bottom, row h_segs = top
    std::vector<std::vector<Vec3>> ring_verts(h_segs + 1, std::vector<Vec3>(n));
    for (int row = 0; row <= h_segs; ++row) {
        float t = static_cast<float>(row) / h_segs; // 0 at bottom, 1 at top
        float y = -hh + t * s.height;
        for (int col = 0; col < n; ++col) {
            float angle = 2.0f * static_cast<float>(M_PI) * col / n;
            ring_verts[row][col] = Vec3(r * std::cos(angle), y, r * std::sin(angle));
        }
    }

    // Lateral quad faces (one quad per segment per height section)
    for (int row = 0; row < h_segs; ++row) {
        for (int col = 0; col < n; ++col) {
            int next_col = (col + 1) % n;
            Face f;
            f.vertices = {
                ring_verts[row][col],
                ring_verts[row][next_col],
                ring_verts[row + 1][next_col],
                ring_verts[row + 1][col]
            };
            // Normal: outward radial direction at midpoint
            Vec3 mid = (ring_verts[row][col] + ring_verts[row][next_col]) * 0.5f;
            f.normal = Vec3(mid.x, 0, mid.z).Normalized();
            faces.push_back(f);
        }
    }

    // Top cap (N-gon, normal +Y, CCW from above)
    Face top_cap;
    top_cap.normal = Vec3(0, 1, 0);
    for (int i = 0; i < n; ++i) {
        top_cap.vertices.push_back(ring_verts[h_segs][i]);
    }
    faces.push_back(top_cap);

    // Bottom cap (N-gon, normal -Y, CCW from below)
    Face bot_cap;
    bot_cap.normal = Vec3(0, -1, 0);
    for (int i = n - 1; i >= 0; --i) {
        bot_cap.vertices.push_back(ring_verts[0][i]);
    }
    faces.push_back(bot_cap);

    return faces;
}

std::vector<Face> FaceGenerator::GenerateSphere(const ShapeParams& s) const
{
    std::vector<Face> faces;
    int slices = s.lon_segments;
    int stacks = s.lat_segments;
    float r = s.radius;

    // Generate vertex grid
    // stacks+1 rows of vertices, slices columns
    auto vertex = [&](int stack, int slice) -> Vec3 {
        float phi = static_cast<float>(M_PI) * stack / stacks;   // 0 at top pole, PI at bottom pole
        float theta = 2.0f * static_cast<float>(M_PI) * slice / slices;
        float sp = std::sin(phi);
        return Vec3(
            r * sp * std::cos(theta),
            r * std::cos(phi),
            r * sp * std::sin(theta)
        );
    };

    // Top pole triangles (stack 0 to stack 1)
    for (int j = 0; j < slices; ++j) {
        int jn = (j + 1) % slices;
        Face f;
        Vec3 v0 = vertex(0, j);    // top pole
        Vec3 v1 = vertex(1, j);
        Vec3 v2 = vertex(1, jn);
        f.vertices = {v0, v1, v2};
        // Normal: outward from center
        Vec3 center = (v0 + v1 + v2) * (1.0f / 3.0f);
        f.normal = center.Normalized();
        faces.push_back(f);
    }

    // Middle quads (stack i to stack i+1, for i in 1..stacks-2)
    for (int i = 1; i < stacks - 1; ++i) {
        for (int j = 0; j < slices; ++j) {
            int jn = (j + 1) % slices;
            Face f;
            Vec3 v0 = vertex(i, j);
            Vec3 v1 = vertex(i + 1, j);
            Vec3 v2 = vertex(i + 1, jn);
            Vec3 v3 = vertex(i, jn);
            f.vertices = {v0, v1, v2, v3};
            Vec3 center = (v0 + v1 + v2 + v3) * 0.25f;
            f.normal = center.Normalized();
            faces.push_back(f);
        }
    }

    // Bottom pole triangles (stack stacks-1 to stack stacks)
    for (int j = 0; j < slices; ++j) {
        int jn = (j + 1) % slices;
        Face f;
        Vec3 v0 = vertex(stacks - 1, j);
        Vec3 v1 = vertex(stacks, j);   // bottom pole
        Vec3 v2 = vertex(stacks - 1, jn);
        f.vertices = {v0, v1, v2};
        Vec3 center = (v0 + v1 + v2) * (1.0f / 3.0f);
        f.normal = center.Normalized();
        faces.push_back(f);
    }

    return faces;
}

std::vector<Face> FaceGenerator::GenerateTorus(const ShapeParams& s) const
{
    std::vector<Face> faces;
    int rings = s.ring_segments;
    int sides = s.side_segments;
    float R = s.major_radius;
    float r = s.minor_radius;

    auto vertex = [&](int ring, int side) -> Vec3 {
        float theta = 2.0f * static_cast<float>(M_PI) * ring / rings;
        float phi = 2.0f * static_cast<float>(M_PI) * side / sides;
        float x = (R + r * std::cos(phi)) * std::cos(theta);
        float y = r * std::sin(phi);
        float z = (R + r * std::cos(phi)) * std::sin(theta);
        return Vec3(x, y, z);
    };

    auto normal_at = [&](int ring, int side) -> Vec3 {
        float theta = 2.0f * static_cast<float>(M_PI) * ring / rings;
        float phi = 2.0f * static_cast<float>(M_PI) * side / sides;
        // Normal points away from the ring center
        float nx = std::cos(phi) * std::cos(theta);
        float ny = std::sin(phi);
        float nz = std::cos(phi) * std::sin(theta);
        return Vec3(nx, ny, nz).Normalized();
    };

    for (int i = 0; i < rings; ++i) {
        int in = (i + 1) % rings;
        for (int j = 0; j < sides; ++j) {
            int jn = (j + 1) % sides;
            Face f;
            f.vertices = {
                vertex(i, j),
                vertex(in, j),
                vertex(in, jn),
                vertex(i, jn)
            };
            // Face normal: average of vertex normals (flat shading approximation)
            Vec3 n0 = normal_at(i, j);
            Vec3 n1 = normal_at(in, j);
            Vec3 n2 = normal_at(in, jn);
            Vec3 n3 = normal_at(i, jn);
            f.normal = (n0 + n1 + n2 + n3).Normalized();
            faces.push_back(f);
        }
    }

    return faces;
}

std::vector<Face> FaceGenerator::GenerateCapsule(const ShapeParams& s) const
{
    std::vector<Face> faces;
    int n = s.segments;
    float r = s.radius;
    float total_h = s.height;
    float cyl_h = total_h - 2.0f * r;
    if (cyl_h < 0.0f) cyl_h = 0.0f;
    float hh = cyl_h * 0.5f;

    // Use half the segments for hemisphere stacks (minimum 2)
    int hemi_stacks = n / 2;
    if (hemi_stacks < 2) hemi_stacks = 2;

    // --- Top hemisphere ---
    // Generate vertices for top hemisphere (y offset = +hh)
    auto top_vertex = [&](int stack, int slice) -> Vec3 {
        // stack 0 = pole (top), stack hemi_stacks = equator
        float phi = 0.5f * static_cast<float>(M_PI) * stack / hemi_stacks; // 0 to PI/2
        float theta = 2.0f * static_cast<float>(M_PI) * slice / n;
        float sp = std::sin(phi);
        float cp = std::cos(phi);
        return Vec3(
            r * sp * std::cos(theta),
            hh + r * cp,
            r * sp * std::sin(theta)
        );
    };

    // Top pole triangles
    for (int j = 0; j < n; ++j) {
        int jn = (j + 1) % n;
        Face f;
        Vec3 v0 = top_vertex(0, j);
        Vec3 v1 = top_vertex(1, j);
        Vec3 v2 = top_vertex(1, jn);
        f.vertices = {v0, v1, v2};
        Vec3 center = (v0 + v1 + v2) * (1.0f / 3.0f);
        Vec3 local_center = center - Vec3(0, hh, 0);
        f.normal = local_center.Normalized();
        faces.push_back(f);
    }

    // Top hemisphere quads
    for (int i = 1; i < hemi_stacks; ++i) {
        for (int j = 0; j < n; ++j) {
            int jn = (j + 1) % n;
            Face f;
            Vec3 v0 = top_vertex(i, j);
            Vec3 v1 = top_vertex(i + 1, j);
            Vec3 v2 = top_vertex(i + 1, jn);
            Vec3 v3 = top_vertex(i, jn);
            f.vertices = {v0, v1, v2, v3};
            Vec3 center = (v0 + v1 + v2 + v3) * 0.25f;
            Vec3 local_center = center - Vec3(0, hh, 0);
            f.normal = local_center.Normalized();
            faces.push_back(f);
        }
    }

    // --- Cylinder middle ---
    if (cyl_h > 0.0f) {
        int h_segs = s.height_segments;
        if (h_segs < 1) h_segs = 1;

        // Generate ring vertices for the cylinder section
        std::vector<std::vector<Vec3>> ring_verts(h_segs + 1, std::vector<Vec3>(n));
        for (int row = 0; row <= h_segs; ++row) {
            float t = static_cast<float>(row) / h_segs; // 0 = bottom, 1 = top
            float y = -hh + t * cyl_h;
            for (int col = 0; col < n; ++col) {
                float angle = 2.0f * static_cast<float>(M_PI) * col / n;
                ring_verts[row][col] = Vec3(r * std::cos(angle), y, r * std::sin(angle));
            }
        }

        for (int row = 0; row < h_segs; ++row) {
            for (int col = 0; col < n; ++col) {
                int next_col = (col + 1) % n;
                Face f;
                f.vertices = {
                    ring_verts[row][col],
                    ring_verts[row][next_col],
                    ring_verts[row + 1][next_col],
                    ring_verts[row + 1][col]
                };
                Vec3 mid = (ring_verts[row][col] + ring_verts[row][next_col]) * 0.5f;
                f.normal = Vec3(mid.x, 0, mid.z).Normalized();
                faces.push_back(f);
            }
        }
    }

    // --- Bottom hemisphere ---
    auto bot_vertex = [&](int stack, int slice) -> Vec3 {
        // stack 0 = equator, stack hemi_stacks = pole (bottom)
        float phi = 0.5f * static_cast<float>(M_PI) * stack / hemi_stacks; // 0 to PI/2
        float theta = 2.0f * static_cast<float>(M_PI) * slice / n;
        float sp = std::sin(phi);
        float cp = std::cos(phi);
        return Vec3(
            r * sp * std::cos(theta),
            -hh - r * cp,  // Note: mirrored (going downward from equator)
            r * sp * std::sin(theta)
        );
    };

    // Actually for bottom hemisphere, we go from equator to bottom pole
    // Let's redefine: stack 0 = equator (at -hh), stack hemi_stacks = bottom pole
    auto bot_vertex2 = [&](int stack, int slice) -> Vec3 {
        // phi goes from PI/2 (equator) to PI (bottom pole)
        float phi = 0.5f * static_cast<float>(M_PI) + 0.5f * static_cast<float>(M_PI) * stack / hemi_stacks;
        float theta = 2.0f * static_cast<float>(M_PI) * slice / n;
        float sp = std::sin(phi);
        float cp = std::cos(phi);
        return Vec3(
            r * sp * std::cos(theta),
            -hh + r * cp, // cp goes from 0 (equator) to -1 (pole)
            r * sp * std::sin(theta)
        );
    };

    // Bottom hemisphere quads (equator to near-pole)
    for (int i = 0; i < hemi_stacks - 1; ++i) {
        for (int j = 0; j < n; ++j) {
            int jn = (j + 1) % n;
            Face f;
            Vec3 v0 = bot_vertex2(i, j);
            Vec3 v1 = bot_vertex2(i + 1, j);
            Vec3 v2 = bot_vertex2(i + 1, jn);
            Vec3 v3 = bot_vertex2(i, jn);
            f.vertices = {v0, v1, v2, v3};
            Vec3 center = (v0 + v1 + v2 + v3) * 0.25f;
            Vec3 local_center = center - Vec3(0, -hh, 0);
            f.normal = local_center.Normalized();
            faces.push_back(f);
        }
    }

    // Bottom pole triangles
    for (int j = 0; j < n; ++j) {
        int jn = (j + 1) % n;
        Face f;
        Vec3 v0 = bot_vertex2(hemi_stacks - 1, j);
        Vec3 v1 = bot_vertex2(hemi_stacks, j); // bottom pole
        Vec3 v2 = bot_vertex2(hemi_stacks - 1, jn);
        f.vertices = {v0, v1, v2};
        Vec3 center = (v0 + v1 + v2) * (1.0f / 3.0f);
        Vec3 local_center = center - Vec3(0, -hh, 0);
        f.normal = local_center.Normalized();
        faces.push_back(f);
    }

    return faces;
}

} // namespace BodyRenderer
