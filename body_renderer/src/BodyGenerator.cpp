#include "BodyGenerator.h"
#include "FaceGenerator.h"
#include <cmath>
#include <algorithm>

namespace BodyRenderer {

// Simple xorshift RNG
unsigned int BodyGenerator::RNG::Next()
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

int BodyGenerator::RNG::IntRange(int min_val, int max_val)
{
    if (min_val >= max_val) return min_val;
    unsigned int range = static_cast<unsigned int>(max_val - min_val + 1);
    return min_val + static_cast<int>(Next() % range);
}

float BodyGenerator::RNG::FloatRange(float min_val, float max_val)
{
    float t = static_cast<float>(Next() & 0xFFFF) / 65535.0f;
    return min_val + t * (max_val - min_val);
}

std::string BodyGenerator::GenerateName(RNG& rng, int depth) const
{
    static const char* prefixes[] = {
        "alpha", "beta", "gamma", "delta", "sigma", "omega",
        "core", "arm", "strut", "ring", "hub", "node",
        "limb", "segment", "module", "pod", "fin", "boom"
    };
    static const int num_prefixes = 18;

    static const char* suffixes[] = {
        "A", "B", "C", "D", "1", "2", "3", "4"
    };
    static const int num_suffixes = 8;

    std::string name = prefixes[rng.IntRange(0, num_prefixes - 1)];
    name += "_";
    name += suffixes[rng.IntRange(0, num_suffixes - 1)];
    name += "_d" + std::to_string(depth);
    return name;
}

Vec3 BodyGenerator::RandomColor(RNG& rng) const
{
    // Generate pleasant colors by picking hue and varying saturation/value
    float hue = rng.FloatRange(0.0f, 360.0f);
    float sat = rng.FloatRange(0.3f, 0.8f);
    float val = rng.FloatRange(0.5f, 0.9f);

    // HSV to RGB conversion
    float c = val * sat;
    float x = c * (1.0f - std::fabs(std::fmod(hue / 60.0f, 2.0f) - 1.0f));
    float m = val - c;

    float r = 0, g = 0, b = 0;
    if (hue < 60)       { r = c; g = x; b = 0; }
    else if (hue < 120) { r = x; g = c; b = 0; }
    else if (hue < 180) { r = 0; g = c; b = x; }
    else if (hue < 240) { r = 0; g = x; b = c; }
    else if (hue < 300) { r = x; g = 0; b = c; }
    else                { r = c; g = 0; b = x; }

    return Vec3(r + m, g + m, b + m);
}

ShapeParams BodyGenerator::RandomShape(RNG& rng, int segments) const
{
    ShapeParams s;
    int type_choice = rng.IntRange(0, 3);

    // All shapes use the same segment count for lateral faces so that
    // face topology (vertex count) matches when connected laterally.
    switch (type_choice) {
    case 0: // Cone
        s.type = ShapeType::Cone;
        s.radius = rng.FloatRange(0.3f, 0.8f);
        s.height = rng.FloatRange(0.5f, 1.5f);
        s.segments = segments;
        break;
    case 1: // Cylinder
        s.type = ShapeType::Cylinder;
        s.radius = rng.FloatRange(0.2f, 0.7f);
        s.height = rng.FloatRange(0.4f, 2.0f);
        s.segments = segments;
        break;
    case 2: // Sphere
        s.type = ShapeType::Sphere;
        s.radius = rng.FloatRange(0.3f, 0.8f);
        s.lon_segments = segments;
        s.lat_segments = segments;
        break;
    case 3: // Torus
        s.type = ShapeType::Torus;
        s.major_radius = rng.FloatRange(0.5f, 1.0f);
        s.minor_radius = rng.FloatRange(0.1f, 0.3f);
        s.ring_segments = segments;
        s.side_segments = segments;
        break;
    }
    return s;
}

Connection BodyGenerator::FindCompatibleConnection(
    RNG& rng,
    const ShapeParams& parent_shape,
    const ShapeParams& child_shape) const
{
    Connection conn;
    conn.type = ConnectionType::Face_Connection;
    conn.rotation = 0.0f;
    conn.offset_u = 0.5f;
    conn.offset_v = 0.5f;

    // Generate faces for both shapes to find compatible pairs
    FaceGenerator faceGen;
    std::vector<Face> parent_faces = faceGen.Generate(parent_shape);
    std::vector<Face> child_faces = faceGen.Generate(child_shape);

    if (parent_faces.empty() || child_faces.empty()) {
        conn.parent_face_index = 0;
        conn.child_face_index = 0;
        return conn;
    }

    // Build a list of compatible face pairs (same vertex count = same topology)
    struct FacePair {
        int parent_idx;
        int child_idx;
    };
    std::vector<FacePair> compatible;

    for (int pi = 0; pi < static_cast<int>(parent_faces.size()); ++pi) {
        size_t pv = parent_faces[pi].vertices.size();
        for (int ci = 0; ci < static_cast<int>(child_faces.size()); ++ci) {
            if (child_faces[ci].vertices.size() == pv && pv >= 3) {
                compatible.push_back({pi, ci});
            }
        }
    }

    if (compatible.empty()) {
        // Fallback: pick any face (will still work but won't be perfect topology match)
        conn.parent_face_index = rng.IntRange(0, static_cast<int>(parent_faces.size()) - 1);
        conn.child_face_index = rng.IntRange(0, static_cast<int>(child_faces.size()) - 1);
    } else {
        // Prefer lateral faces (not caps) for more interesting geometry.
        // Lateral faces are typically the first N faces for cone/cylinder (before caps).
        // Pick a random compatible pair.
        int idx = rng.IntRange(0, static_cast<int>(compatible.size()) - 1);
        conn.parent_face_index = compatible[idx].parent_idx;
        conn.child_face_index = compatible[idx].child_idx;
    }

    // Small random rotation increment aligned to face subdivisions
    // This keeps the geometry on-grid with the subdivision faces
    int rot_steps = rng.IntRange(0, 3);
    conn.rotation = static_cast<float>(rot_steps) * 90.0f;

    return conn;
}

BodyNode BodyGenerator::GenerateNode(RNG& rng, int depth, int max_depth, int segments) const
{
    BodyNode node;
    node.name = GenerateName(rng, depth);
    node.shape = RandomShape(rng, segments);
    node.color = RandomColor(rng);

    if (depth < max_depth) {
        // Number of children decreases with depth
        int max_children = (max_depth - depth);
        if (max_children > 3) max_children = 3;
        int num_children = rng.IntRange(1, max_children);

        for (int i = 0; i < num_children; ++i) {
            BodyNode child = GenerateNode(rng, depth + 1, max_depth, segments);
            child.connection = FindCompatibleConnection(rng, node.shape, child.shape);
            node.children.push_back(child);
        }
    }

    return node;
}

Body BodyGenerator::Generate(unsigned int seed, int depth_limit) const
{
    RNG rng(seed == 0 ? 1 : seed); // avoid zero seed

    Body body;
    body.name = "Generated_" + std::to_string(seed);
    body.material.shininess = rng.FloatRange(16.0f, 80.0f);
    body.material.ambient = Vec3(
        rng.FloatRange(0.05f, 0.2f),
        rng.FloatRange(0.05f, 0.2f),
        rng.FloatRange(0.05f, 0.2f)
    );

    // Use a consistent segment count for all shapes in this body.
    // This ensures lateral faces have compatible topology (same vertex count).
    int segments = rng.IntRange(8, 12);

    body.root = GenerateNode(rng, 0, depth_limit, segments);

    return body;
}

} // namespace BodyRenderer
