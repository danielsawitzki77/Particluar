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
        return shape.segments + 1;
    case ShapeType::Cylinder:
        return shape.segments + 2;
    case ShapeType::Sphere:
        return shape.lon_segments * shape.lat_segments;
    case ShapeType::Torus:
        return shape.ring_segments * shape.side_segments;
    case ShapeType::Capsule:
        return shape.segments * 2 + shape.segments; // hemispheres + cylinder
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
    float hue = rng.FloatRange(0.0f, 360.0f);
    float sat = rng.FloatRange(0.3f, 0.8f);
    float val = rng.FloatRange(0.5f, 0.9f);

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
    int type_choice = rng.IntRange(0, 4); // now includes capsule

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
    case 4: // Capsule
        s.type = ShapeType::Capsule;
        s.radius = rng.FloatRange(0.15f, 0.5f);
        s.height = s.radius * 2.0f + rng.FloatRange(0.3f, 1.5f); // ensure height >= 2*radius
        s.segments = rng.IntRange(6, 20);
        break;
    }
    return s;
}

Connection BodyGenerator::RandomConnection(RNG& rng, const ShapeParams& parent_shape) const
{
    Connection conn;
    conn.is_legacy = false; // generate v2 parametric connections

    // Select a valid region for the parent shape
    switch (parent_shape.type) {
    case ShapeType::Sphere:
        conn.parent_attach.region = AttachRegion::Surface;
        conn.parent_attach.u = rng.FloatRange(0.0f, 1.0f);
        conn.parent_attach.v = rng.FloatRange(0.1f, 0.9f); // avoid exact poles
        break;
    case ShapeType::Cylinder: {
        int region_choice = rng.IntRange(0, 2);
        if (region_choice == 0) {
            conn.parent_attach.region = AttachRegion::Top;
            conn.parent_attach.u = 0.5f;
            conn.parent_attach.v = 0.5f;
        } else if (region_choice == 1) {
            conn.parent_attach.region = AttachRegion::Bottom;
            conn.parent_attach.u = 0.5f;
            conn.parent_attach.v = 0.5f;
        } else {
            conn.parent_attach.region = AttachRegion::Side;
            conn.parent_attach.u = rng.FloatRange(0.0f, 1.0f);
            conn.parent_attach.v = rng.FloatRange(0.2f, 0.8f);
        }
        break;
    }
    case ShapeType::Cone: {
        int region_choice = rng.IntRange(0, 1);
        if (region_choice == 0) {
            conn.parent_attach.region = AttachRegion::Base;
            conn.parent_attach.u = 0.5f;
            conn.parent_attach.v = 0.5f;
        } else {
            conn.parent_attach.region = AttachRegion::Side;
            conn.parent_attach.u = rng.FloatRange(0.0f, 1.0f);
            conn.parent_attach.v = rng.FloatRange(0.1f, 0.6f); // lower half of cone side
        }
        break;
    }
    case ShapeType::Torus:
        conn.parent_attach.region = AttachRegion::Surface;
        conn.parent_attach.u = rng.FloatRange(0.0f, 1.0f);
        conn.parent_attach.v = rng.FloatRange(0.0f, 1.0f);
        break;
    case ShapeType::Capsule: {
        int region_choice = rng.IntRange(0, 2);
        if (region_choice == 0) {
            conn.parent_attach.region = AttachRegion::TopCap;
            conn.parent_attach.u = 0.5f;
            conn.parent_attach.v = 0.0f; // pole
        } else if (region_choice == 1) {
            conn.parent_attach.region = AttachRegion::BottomCap;
            conn.parent_attach.u = 0.5f;
            conn.parent_attach.v = 0.0f;
        } else {
            conn.parent_attach.region = AttachRegion::Side;
            conn.parent_attach.u = rng.FloatRange(0.0f, 1.0f);
            conn.parent_attach.v = rng.FloatRange(0.2f, 0.8f);
        }
        break;
    }
    }

    // Child attachment — typically connect at top or bottom for directional shapes
    // For spheres, use surface bottom pole
    conn.child_attach.u = 0.5f;
    conn.child_attach.v = 0.5f;

    // Pick a sensible child attachment based on child shape would be ideal,
    // but we don't know the child shape at this point — use defaults
    // that work for most shapes (top/bottom/surface center)
    int child_choice = rng.IntRange(0, 1);
    if (child_choice == 0) {
        conn.child_attach.region = AttachRegion::Top;
    } else {
        conn.child_attach.region = AttachRegion::Bottom;
    }

    // Rotation is only valid for cap↔cap connections.
    // For side/surface connections, face grid alignment determines orientation.
    bool is_side = (conn.parent_attach.region == AttachRegion::Side ||
                    conn.parent_attach.region == AttachRegion::Surface);
    if (is_side) {
        conn.rotation = 0.0f;
    } else {
        conn.rotation = static_cast<float>(rng.IntRange(0, 7)) * 45.0f;
    }

    return conn;
}

// Fix up child attachment region to be valid for the child's shape type
// AND enforce topology compatibility (quad↔quad, ngon↔ngon)
static void FixChildAttachment(Connection& conn, const ShapeParams& child_shape)
{
    AttachRegion parent_region = conn.parent_attach.region;

    // Determine if parent produces quads or ngons at its attachment
    bool parent_is_quad = (parent_region == AttachRegion::Side ||
                           parent_region == AttachRegion::Surface);

    switch (child_shape.type) {
    case ShapeType::Sphere:
        conn.child_attach.region = AttachRegion::Surface;
        conn.child_attach.u = 0.5f;
        conn.child_attach.v = 0.5f; // equator (quad), NOT pole (triangle)
        break;
    case ShapeType::Cylinder:
        if (parent_is_quad) {
            // Parent has quads — child must use Side (quad)
            conn.child_attach.region = AttachRegion::Side;
            conn.child_attach.u = 0.5f;
            conn.child_attach.v = 0.2f;  // NOT 0.0 — edge produces degenerate faces
        } else {
            // Parent has ngons — child uses cap (ngon)
            conn.child_attach.region = AttachRegion::Bottom;
            conn.child_attach.u = 0.5f;
            conn.child_attach.v = 0.5f;
        }
        break;
    case ShapeType::Cone:
        // Cone base is ngon, cone side is triangle
        // For quad parents: no compatible cone face exists — use base anyway
        conn.child_attach.region = AttachRegion::Base;
        conn.child_attach.u = 0.5f;
        conn.child_attach.v = 0.5f;
        break;
    case ShapeType::Torus:
        conn.child_attach.region = AttachRegion::Surface;
        conn.child_attach.u = 0.5f;
        conn.child_attach.v = 0.5f;
        break;
    case ShapeType::Capsule:
        if (parent_is_quad) {
            conn.child_attach.region = AttachRegion::Side;
            conn.child_attach.u = 0.5f;
            conn.child_attach.v = 0.2f;  // NOT 0.0 — edge produces degenerate faces
        } else {
            conn.child_attach.region = AttachRegion::BottomCap;
            conn.child_attach.u = 0.5f;
            conn.child_attach.v = 0.5f;
        }
        break;
    }

    // Lock rotation to 0 for side/surface connections
    bool is_side = (conn.child_attach.region == AttachRegion::Side ||
                    conn.parent_attach.region == AttachRegion::Surface ||
                    conn.parent_attach.region == AttachRegion::Side);
    if (is_side) {
        conn.rotation = 0.0f;
    }
}

BodyNode BodyGenerator::GenerateNode(RNG& rng, int depth, int max_depth) const
{
    BodyNode node;
    node.name = GenerateName(rng, depth);
    node.shape = RandomShape(rng);
    node.color = RandomColor(rng);

    if (depth < max_depth) {
        int max_children = (max_depth - depth);
        if (max_children > 4) max_children = 4;
        int num_children = rng.IntRange(0, max_children);

        for (int i = 0; i < num_children; ++i) {
            BodyNode child = GenerateNode(rng, depth + 1, max_depth);
            child.connection = RandomConnection(rng, node.shape);

            // Enforce topology: if parent attachment is quad, child must produce quads.
            // Cone has no quad faces (side=triangles, base=ngon) so replace with cylinder.
            bool parent_is_quad = (child.connection.parent_attach.region == AttachRegion::Side ||
                                   child.connection.parent_attach.region == AttachRegion::Surface);
            if (parent_is_quad && child.shape.type == ShapeType::Cone) {
                // Cone can't produce quad faces — switch to cylinder (same visual, has Side quads)
                child.shape.type = ShapeType::Cylinder;
                child.shape.height = child.shape.radius * 2.0f;
            }

            // Fix up the child attachment to be valid for the child's actual shape
            FixChildAttachment(child.connection, child.shape);

            // Enforce topology: after FixChildAttachment, verify quad↔quad compatibility
            bool child_is_quad = (child.connection.child_attach.region == AttachRegion::Side ||
                                  child.connection.child_attach.region == AttachRegion::Surface);
            if (parent_is_quad && !child_is_quad) {
                // Force child to use Side region for quad compatibility
                child.connection.child_attach.region = AttachRegion::Side;
                child.connection.child_attach.u = 0.5f;
                child.connection.child_attach.v = 0.2f;
                child.connection.rotation = 0.0f;
            }

            // Cap connections: match child radius to parent to avoid holes
            if (!parent_is_quad) {
                if (child.shape.radius < node.shape.radius * 0.9f) {
                    child.shape.radius = node.shape.radius;
                }
            }

            node.children.push_back(child);
        }
    }

    return node;
}

Body BodyGenerator::Generate(unsigned int seed, int depth_limit) const
{
    RNG rng(seed == 0 ? 1 : seed);

    Body body;
    body.name = "Generated_" + std::to_string(seed);
    body.format_version = 2;
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
