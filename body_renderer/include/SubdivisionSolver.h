#pragma once

#include "BodyTypes.h"

namespace BodyRenderer {

// Computes subdivision parameters for an entire body tree based on
// connection positions and a base resolution. Ensures that:
// - Angular segments match at connection boundaries
// - Height segments produce face boundaries at connection v-positions
// - Non-uniform row spacing matches connected shape face heights
// - Sphere lat_segments and torus ring/side_segments are derived correctly
class SubdivisionSolver {
public:
    // Derive all subdivision parameters for the body tree.
    // base_resolution is the minimum segment count (e.g., 8 for level 1, 16 for level 2).
    void Solve(Body& body, int base_resolution) const;

    // Convenience: solve + resolve connections in one call.
    // This is the typical entry point for any consumer.
    void PrepareBody(Body& body, int base_resolution) const;
};

} // namespace BodyRenderer
