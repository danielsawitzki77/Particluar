#include "FaceGenerator.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace BodyRenderer {

std::vector<Face> FaceGenerator::Generate(const ShapeParams& shape) const
{
    switch (shape.type) {
    case ShapeType::Box:      return GenerateBox(shape);
    case ShapeType::Cone:     return GenerateCone(shape);
    case ShapeType::Cylinder: return GenerateCylinder(shape);
    case ShapeType::Sphere:   return GenerateSphere(shape);
    case ShapeType::Torus:    return GenerateTorus(shape);
    case ShapeType::Frustum:  return GenerateFrustum(shape);
    }
    return {};
}

std::vector<Face> FaceGenerator::GenerateBox(const ShapeParams& s) const
{
    std::vector<Face> faces;
    float hw = s.width * 0.5f;
    float hh = s.height * 0.5f;
    float hd = s.depth * 0.5f;

    // 8 vertices
    Vec3 v0(-hw, -hh, -hd);
    Vec3 v1( hw, -hh, -hd);
    Vec3 v2( hw,  hh, -hd);
    Vec3 v3(-hw,  hh, -hd);
    Vec3 v4(-hw, -hh,  hd);
    Vec3 v5( hw, -hh,  hd);
    Vec3 v6( hw,  hh,  hd);
    Vec3 v7(-hw,  hh,  hd);

    // Front face (z+) - CCW from outside
    Face front;
    front.vertices = {v4, v5, v6, v7};
    front.normal = Vec3(0, 0, 1);
    faces.push_back(front);

    // Back face (z-) - CCW from outside
    Face back;
    back.vertices = {v1, v0, v3, v2};
    back.normal = Vec3(0, 0, -1);
    faces.push_back(back);

    // Top face (y+) - CCW from outside
    Face top;
    top.vertices = {v3, v7, v6, v2};
    top.normal = Vec3(0, 1, 0);
    faces.push_back(top);

    // Bottom face (y-) - CCW from outside
    Face bottom;
    bottom.vertices = {v0, v1, v5, v4};
    bottom.normal = Vec3(0, -1, 0);
    faces.push_back(bottom);

    // Right face (x+) - CCW from outside
    Face right;
    right.vertices = {v5, v1, v2, v6};
    right.normal = Vec3(1, 0, 0);
    faces.push_back(right);

    // Left face (x-) - CCW from outside
    Face left;
    left.vertices = {v0, v4, v7, v3};
    left.normal = Vec3(-1, 0, 0);
    faces.push_back(left);

    return faces;
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

    // Generate circle vertices
    std::vector<Vec3> top_verts(n), bot_verts(n);
    for (int i = 0; i < n; ++i) {
        float angle = 2.0f * static_cast<float>(M_PI) * i / n;
        float cx = r * std::cos(angle);
        float cz = r * std::sin(angle);
        top_verts[i] = Vec3(cx, hh, cz);
        bot_verts[i] = Vec3(cx, -hh, cz);
    }

    // Lateral quad faces
    for (int i = 0; i < n; ++i) {
        int next = (i + 1) % n;
        Face f;
        f.vertices = {bot_verts[i], bot_verts[next], top_verts[next], top_verts[i]};

        // Normal: outward radial direction at midpoint
        Vec3 mid = (bot_verts[i] + bot_verts[next]) * 0.5f;
        f.normal = Vec3(mid.x, 0, mid.z).Normalized();
        faces.push_back(f);
    }

    // Top cap (N-gon, normal +Y, CCW from above)
    Face top_cap;
    top_cap.normal = Vec3(0, 1, 0);
    for (int i = 0; i < n; ++i) {
        top_cap.vertices.push_back(top_verts[i]);
    }
    faces.push_back(top_cap);

    // Bottom cap (N-gon, normal -Y, CCW from below)
    Face bot_cap;
    bot_cap.normal = Vec3(0, -1, 0);
    for (int i = n - 1; i >= 0; --i) {
        bot_cap.vertices.push_back(bot_verts[i]);
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

std::vector<Face> FaceGenerator::GenerateFrustum(const ShapeParams& s) const
{
    std::vector<Face> faces;
    int n = s.segments;
    float rt = s.top_radius;
    float rb = s.bottom_radius;
    float hh = s.height * 0.5f;

    // Generate circle vertices
    std::vector<Vec3> top_verts(n), bot_verts(n);
    for (int i = 0; i < n; ++i) {
        float angle = 2.0f * static_cast<float>(M_PI) * i / n;
        float ca = std::cos(angle);
        float sa = std::sin(angle);
        top_verts[i] = Vec3(rt * ca, hh, rt * sa);
        bot_verts[i] = Vec3(rb * ca, -hh, rb * sa);
    }

    if (rt > 0.0f) {
        // Lateral quad faces
        for (int i = 0; i < n; ++i) {
            int next = (i + 1) % n;
            Face f;
            f.vertices = {bot_verts[i], bot_verts[next], top_verts[next], top_verts[i]};

            // Compute outward normal for frustum side
            Vec3 e1 = bot_verts[next] - bot_verts[i];
            Vec3 e2 = top_verts[i] - bot_verts[i];
            f.normal = e1.Cross(e2).Normalized();
            faces.push_back(f);
        }

        // Top cap (N-gon, normal +Y)
        Face top_cap;
        top_cap.normal = Vec3(0, 1, 0);
        for (int i = 0; i < n; ++i) {
            top_cap.vertices.push_back(top_verts[i]);
        }
        faces.push_back(top_cap);
    } else {
        // Degenerate to cone — triangular lateral faces
        Vec3 tip(0, hh, 0);
        for (int i = 0; i < n; ++i) {
            int next = (i + 1) % n;
            Face f;
            f.vertices = {bot_verts[i], bot_verts[next], tip};
            Vec3 e1 = bot_verts[next] - bot_verts[i];
            Vec3 e2 = tip - bot_verts[i];
            f.normal = e1.Cross(e2).Normalized();
            faces.push_back(f);
        }
    }

    // Bottom cap (N-gon, normal -Y)
    Face bot_cap;
    bot_cap.normal = Vec3(0, -1, 0);
    for (int i = n - 1; i >= 0; --i) {
        bot_cap.vertices.push_back(bot_verts[i]);
    }
    faces.push_back(bot_cap);

    return faces;
}

} // namespace BodyRenderer
