#pragma once

#include "BodyTypes.h"
#include <string>

namespace BodyRenderer {

// Procedural body generator — creates random bodies from a seed.
// Generated bodies connect child shapes at matching subdivision faces:
// faces share the same polygon topology (vertex count) and are scaled
// to exactly overlap, ensuring bodies touch at one shared face without
// volume intersection.
class BodyGenerator {
public:
    // Generate a random body using the given seed.
    // depth_limit caps the tree depth (1 = root only, 2 = root + children, etc.)
    Body Generate(unsigned int seed, int depth_limit = 4) const;

private:
    struct RNG {
        unsigned int state;
        RNG(unsigned int seed) : state(seed) {}
        unsigned int Next();
        int IntRange(int min_val, int max_val);
        float FloatRange(float min_val, float max_val);
    };

    BodyNode GenerateNode(RNG& rng, int depth, int max_depth, int segments) const;
    ShapeParams RandomShape(RNG& rng, int segments) const;
    Vec3 RandomColor(RNG& rng) const;
    Connection FindCompatibleConnection(RNG& rng, const ShapeParams& parent_shape, const ShapeParams& child_shape) const;
    std::string GenerateName(RNG& rng, int depth) const;
};

} // namespace BodyRenderer
