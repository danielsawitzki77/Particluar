#include "BodyGenerator.h"
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

int BodyGenerator::EstimateFaceCount(const ShapeParams& shape) const
{
    switch (shape.type) {
    case ShapeType::Cone:
        // sides + 1 base face
        return shape.segments + 1;
    case ShapeType::Cylinder:
        // sides + 2 cap faces
        return shape.segments + 2;
    case ShapeType::Sphere:
        return shape.lon_segments * shape.lat_segments;
    case ShapeType::Torus:
        return shape.ring_segments * shape.side_segments;
    }
    return 8;
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

ShapeParams BodyGenerator::RandomShape(RNG& rng) const
{
    ShapeParams s;
    int type_choice = rng.IntRange(0, 3);

    switch (type_choice) {
    case 0: // Cone
        s.type = ShapeType::Cone;
        s.radius = rng.FloatRange(0.2f, 0.8f);
        s.height = rng.FloatRange(0.5f, 1.5f);
        s.segments = rng.IntRange(6, 20);
        break;
    case 1: // Cylinder
        s.type = ShapeType::Cylinder;
        s.radius = rng.FloatRange(0.15f, 0.7f);
        s.height = rng.FloatRange(0.4f, 2.0f);
        s.segments = rng.IntRange(6, 20);
        break;
    case 2: // Sphere
        s.type = ShapeType::Sphere;
        s.radius = rng.FloatRange(0.2f, 0.8f);
        s.lon_segments = rng.IntRange(8, 20);
        s.lat_segments = rng.IntRange(6, 14);
        break;
    case 3: // Torus
        s.type = ShapeType::Torus;
        s.major_radius = rng.FloatRange(0.5f, 1.2f);
        s.minor_radius = rng.FloatRange(0.1f, s.major_radius * 0.4f);
        s.ring_segments = rng.IntRange(8, 20);
        s.side_segments = rng.IntRange(6, 12);
        break;
    }
    return s;
}

Connection BodyGenerator::RandomConnection(RNG& rng, const ShapeParams& parent_shape) const
{
    Connection conn;

    // Prefer face connections — they ensure shapes share faces without intersecting
    int type_choice = rng.IntRange(0, 4);
    if (type_choice <= 2) {
        conn.type = ConnectionType::Face_Connection;
    } else if (type_choice == 3) {
        conn.type = ConnectionType::Point_Connection;
    } else {
        conn.type = ConnectionType::Edge_Connection;
    }

    int face_count = EstimateFaceCount(parent_shape);
    if (face_count < 1) face_count = 1;

    conn.parent_face_index = rng.IntRange(0, face_count - 1);
    conn.child_face_index = rng.IntRange(0, 3); // usually low index for child alignment
    conn.offset_u = rng.FloatRange(0.3f, 0.7f);
    conn.offset_v = rng.FloatRange(0.3f, 0.7f);
    conn.rotation = static_cast<float>(rng.IntRange(0, 7)) * 45.0f;

    return conn;
}

BodyNode BodyGenerator::GenerateNode(RNG& rng, int depth, int max_depth) const
{
    BodyNode node;
    node.name = GenerateName(rng, depth);
    node.shape = RandomShape(rng);
    node.color = RandomColor(rng);

    if (depth < max_depth) {
        // Number of children decreases with depth
        int max_children = (max_depth - depth);
        if (max_children > 4) max_children = 4;
        int num_children = rng.IntRange(0, max_children);

        for (int i = 0; i < num_children; ++i) {
            BodyNode child = GenerateNode(rng, depth + 1, max_depth);
            child.connection = RandomConnection(rng, node.shape);
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

    body.root = GenerateNode(rng, 0, depth_limit);

    return body;
}

} // namespace BodyRenderer
