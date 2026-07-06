#pragma once

#include "BodyTypes.h"
#include <string>

namespace BodyRenderer {

// Procedural body generator — creates random bodies from a seed.
// Generated bodies connect child shapes at parent subdivision faces
// (no interpenetration, only shared faces).
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

    BodyNode GenerateNode(RNG& rng, int depth, int max_depth) const;
    ShapeParams RandomShape(RNG& rng) const;
    Vec3 RandomColor(RNG& rng) const;
    Connection RandomConnection(RNG& rng, const ShapeParams& parent_shape) const;
    int EstimateFaceCount(const ShapeParams& shape) const;
    std::string GenerateName(RNG& rng, int depth) const;
};

} // namespace BodyRenderer
